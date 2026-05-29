# Concurrency Guide

This document explains the agent engine's four-level concurrency model,
the BatchExecutor algorithm, thread-safety guarantees, cancellation
propagation, starvation avoidance, and rules for writing concurrent Actions.

---

## The Four Levels

### L1 — Many agents, shared pool

```
Thread Pool (N threads)
  ├── Agent A  → run() blocking on worker thread
  ├── Agent B  → run() blocking on worker thread
  ├── Agent C  → run() blocking on worker thread
  └── …
```

`AgentManager::runAgent()` submits each agent's `run()` to the shared
`ThreadPool`. Agents share nothing except:
- `Blackboard` (Pattern C — mutex-protected)
- `EventBus` (subscriber list — mutex-protected)
- `WorkFactory` (registry — shared_mutex-protected)
- `PromptLoader` (cache — mutex-protected)
- `QuotaManager` (counters — mutex-protected)

All per-agent state (queue, history, cancellation flag) is owned exclusively by
that agent's `AgentContext`.

### L2 — Sub-agents (Pattern A fan-out)

```
Agent A (worker thread)
  └── SpawnChildAction::execute()
        ├── mgr->spawnAgent(child_cfg)
        └── mgr->runAgentBlocking(child_id)  ← occupies a pool thread!
              └── Agent B (new pool thread)
```

**Starvation risk:** if agent A blocks a pool thread waiting for agent B, and
agent B's child blocks another pool thread, you can exhaust the pool.

Rule: `thread_pool_size ≥ max_agent_depth + 1` to be safe.

Alternatively, use the non-blocking `fanOut()` + `fanIn()` pattern:

```cpp
auto futures = mgr.fanOut(child_configs, shared_task);
// caller thread is free to do other work here
auto synth_result = mgr.fanIn(futures, synthesiser_config);
```

`fanIn()` calls `futures[i].get()` sequentially without holding a pool thread,
so only one pool thread is occupied per child completion.

### L3 — BatchExecutor DAG within one agent

After a Stage pushes a plan, the agent loop drains all queued items and runs
them as a single batch via `BatchExecutor`. Independent items are submitted to
the shared pool concurrently; dependent items are held until their prerequisites
complete.

```
Agent A (on pool thread T1)
  BatchExecutor.execute([a, b(dep a), c(dep a), d(dep b,c)])
    ├── T1: submit a
    ├── a done → T2: submit b, T3: submit c  (concurrent)
    ├── b done (c still running)
    ├── c done → T4: submit d
    └── d done → results folded into history in declared order
```

### L4 — Sequential reasoning (Stage runs one-at-a-time per agent)

Stages always run as a single-item batch (batch size 1). This gives each Stage
a stable, consistent view of the full history before it makes decisions. Only
after the stage returns does the plan batch execute (potentially in parallel).

---

## BatchExecutor Algorithm — Step-by-Step

**Inputs:** `batch` = ordered list of `WorkItem`, `ctx` = `AgentContext`.

```
Items: [a, b(deps a), c(deps a), d(deps b,c)]

1. Build id→index map:   {a:0, b:1, c:2, d:3}

2. Build edges:
   b.deps = {a} → in-batch → edge: nodes[0].dependents.push_back(1)
                              nodes[1].pending_deps = {a}
   c.deps = {a} → in-batch → edge: nodes[0].dependents.push_back(2)
                              nodes[2].pending_deps = {a}
   d.deps = {b,c} → in-batch → edges: nodes[1].dependents.push_back(3)
                                       nodes[2].dependents.push_back(3)
                               nodes[3].pending_deps = {b,c}

3. Kahn's cycle check (scratch in-degrees):
   in_degree = [0, 1, 1, 2]
   ready_q = {0}     (a has in_degree 0)
   Process 0(a) → in_degree[1]--, in_degree[2]-- → ready_q = {1, 2}
   Process 1(b) → in_degree[3]-- → ready_q = {2, 3? no, still 1}
   Process 2(c) → in_degree[3]-- → ready_q = {3}
   Process 3(d) → done
   processed=4 == n=4 → no cycle

4. Initial ready set: nodes with pending_deps.empty():  {a}

5. Execute loop:
   Iteration 1:
     ready = {a}
     Submit a to pool → in_flight = {a}
     a completes → result[0] = a_result
     for dep 1(b): pending_deps.erase(a) → empty → ready.push_back(b)
     for dep 2(c): pending_deps.erase(a) → empty → ready.push_back(c)

   Iteration 2:
     ready = {b, c}
     Submit b, c to pool concurrently → in_flight = {b, c}
     (b and c run in parallel on different pool threads)
     b completes first → result[1] = b_result
       for dep 3(d): pending_deps.erase(b) → {c} ≠ empty → not ready
     c completes → result[2] = c_result
       for dep 3(d): pending_deps.erase(c) → {} = empty → ready.push_back(d)

   Iteration 3:
     ready = {d}
     Submit d → in_flight = {d}
     d completes → result[3] = d_result

6. Deterministic merge: fold results[0..3] into ctx.history() in declared order
   → history: [a, b, c, d]  (NOT completion order a,c,b,d or similar)
```

---

## What IS and ISN'T Thread-Safe

### IS thread-safe (callable from any thread)

| Call | Primitive |
|------|-----------|
| `AgentManager::spawnAgent()` | `m_agents_mutex` |
| `AgentManager::cancelAgent()` | `m_agents_mutex` |
| `AgentManager::injectWork()` | `m_agents_mutex` + `AgentContext::m_queue_mutex` |
| `AgentManager::sendMessage()` | `m_agents_mutex` |
| `AgentManager::blackboardWrite/Read()` | `Blackboard::m_mutex` |
| `Blackboard::write/read/keys()` | `Blackboard::m_mutex` |
| `EventBus::emit()` | copies subscriber list under `m_mutex`, dispatches outside lock |
| `EventBus::subscribe/unsubscribe()` | `m_mutex` |
| `WorkFactory::create()` | `shared_lock(m_mutex)` |
| `WorkFactory::registerItem()` | `unique_lock(m_mutex)` |
| `AgentContext::injectFromOutside()` | `m_queue_mutex` + `m_queue_cv.notify_one()` |
| `AgentContext::cancellation_flag` (read/write) | `std::atomic<bool>` |
| `ThreadPool::submit()` | `m_mutex` |
| `MessageInbox::push/drain()` | `m_mutex` |
| `QuotaManager::tryAcquire/release()` | `m_mutex` |

### IS NOT thread-safe (call from the agent's execution thread only)

| Call | Reason |
|------|--------|
| `AgentContext::push()` (non-external) | Intended for in-agent use; use `injectFromOutside()` from other threads |
| `AgentContext::should_stop = true` | Not atomic — set only from the agent's own execute() calls |
| `AgentContext::final_output =` | Not atomic — set only from the agent's own execute() calls |
| `WorkItem::inputs =` (in BatchExecutor) | BatchExecutor mutates inputs pre-execution; items are not shared across threads |

---

## Cancellation Propagation

```
mgr.cancelAgent("agent_1")
    ↓
    agent_1.context().cancellation_flag = true  (atomic store)
    agent_1.context().wakeLoop()                (notify condvar)
    ↓
Agent::run() loop checks flag at each iteration start:
    if (ctx.cancellation_flag.load()) return Cancelled;
    ↓
BatchExecutor::execute() checks flag before submitting each item:
    if (ctx.cancellation_flag.load()) → mark remaining items as cancelled
    ↓
In-flight futures are allowed to complete (not interrupted mid-execute).
Their results are discarded (recorded with success=false, error="Cancelled").
    ↓
Sub-agents spawned by actions: their cancellation_flag is NOT automatically
set. The action must propagate cancellation explicitly:
    if (ctx.cancellation_flag.load()) mgr->cancelAgent(child_id);
```

---

## How to Avoid Pool Starvation

### Rule 1: `thread_pool_size ≥ nesting_depth + 1`

If you nest 3 levels of sub-agents (parent → child → grandchild), you need at
least 4 pool threads. Each blocking `runAgentBlocking()` call occupies one
thread.

### Rule 2: Use `fanOut()` + `fanIn()` for wide fan-outs

`fanOut()` returns futures immediately. The calling thread can then call
`fanIn()` which waits for futures using `std::future::get()` — this releases
the calling thread between completions.

### Rule 3: Avoid blocking the pool thread for I/O longer than ~500ms

Long-blocking actions (network requests, large file reads) can stall the pool.
Either:
- Use a separate I/O thread and signal via a condition variable, or
- Accept the latency and size the pool accordingly.

---

## Rules for Writing a New Concurrent Action

1. **No static mutable state without a mutex.**
   Static variables shared across action instances are a data race. Use
   `std::mutex`-protected singletons or pass shared state via the constructor.

2. **Do not block the pool thread for more than a few hundred milliseconds.**
   The pool is shared. A 10-second blocking operation in one action starves all
   other concurrently-ready items.

3. **Check `ctx.cancellation_flag` periodically in long loops.**
   ```cpp
   for (auto& chunk : large_dataset) {
       if (ctx.cancellation_flag.load()) break;
       process(chunk);
   }
   ```

4. **Exclusive file paths: document them.**
   If your action reads/writes a specific file path, and two instances of your
   action can run concurrently, you must either:
   - Guard the path with a mutex, or
   - Document "this action must not be used in the same batch as another
     instance of itself" in the action's `description()`.

5. **Do not call `ctx.push()` from parallel action threads.**
   `push()` (non-external) is for the agent's own execute thread. Only
   Stages (which run single-threaded) should call `ctx.push()`. Actions
   that need to push follow-ups should return their desired follow-ups in
   `WorkResult::output` and have a subsequent Stage process them.

6. **Memory ordering:** `cancellation_flag` uses `std::atomic<bool>`. For
   other shared counters, prefer `std::atomic<int>` with the default
   `memory_order_seq_cst` unless you have profiling evidence to do otherwise.
