# Agent

`include/agent/agent.hpp` · `src/agent/agent.cpp`

> The companion runtime-state object has its own page: [AgentContext](agent_context.md). This page covers the `Agent` loop; the section below is a summary — see [agent_context.md](agent_context.md) for the full queue/history/`$ref` reference.

---

## Agent

### Overview

`Agent` owns an `AgentContext` and runs the event loop. It drives the agent from an initial task to a terminal state by repeatedly popping batches from the queue, executing them through `BatchExecutor`, and recording results into history.

One `Agent` per active task. Its `run()` method executes on a **dedicated `std::thread`** managed by `AgentManager`, keeping the shared `ThreadPool` free for intra-batch parallelism.

### Loop Invariant

```
while not terminated:
    batch = drain(queue)              // pop() blocks; try_pop() drains remainder
    results = BatchExecutor.execute(batch)
    ctx.history += results (declared order)
    check termination
```

Stages execute sequentially within a batch (each sees full prior history before the next one runs). Actions may execute in parallel when their `$ref` dependencies allow it.

### Construction

```cpp
explicit Agent(std::unique_ptr<AgentContext> ctx, ThreadPool& pool);
```

Takes ownership of the context; the pool is used by the internal `BatchExecutor`.

### `run()`

```cpp
RunResult run();
```

Blocking. Runs until one of the termination conditions fires, then returns `RunResult`.

### `RunResult`

```cpp
struct RunResult {
    TerminationReason reason;
    nlohmann::json    output;
    int               iterations{0};
    std::string       error;
};
```

### `TerminationReason`

| Value | Meaning |
|---|---|
| `QueueEmpty` | Queue drained and idle grace period expired (normal completion) |
| `ShouldStop` | `AgentContext::should_stop` was set by a stage |
| `MaxIterations` | `config.max_iterations` was reached |
| `Cancelled` | `AgentContext::cancellation_flag` was set externally |
| `Error` | Unhandled exception escaped a work item |

### Context Access

```cpp
AgentContext&       context();
const AgentContext& context() const;
```

---

## AgentContext

### Overview

`AgentContext` is the complete per-agent runtime state. It decouples the agent loop from `AgentManager` on the hot path: everything the loop needs is carried here directly — queue, history, LLM client, factory, prompt loader, blackboard pointer, inbox pointer, event bus pointer, and config.

### Configuration

```cpp
struct AgentConfig {
    std::string    agent_id;
    std::string    name;
    std::string    task;
    int            max_iterations{100};
    int            max_depth{3};
    int            current_depth{0};
    nlohmann::json extra;         // user-defined passthrough
};
```

### Construction

```cpp
explicit AgentContext(
    AgentConfig                    config,
    std::shared_ptr<LLMClient>     llm,
    std::shared_ptr<WorkFactory>   factory,
    std::shared_ptr<PromptLoader>  prompt_loader,
    std::shared_ptr<MemoryBackend> memory,
    Blackboard*                    blackboard,
    MessageInbox*                  inbox,
    EventBus*                      event_bus,
    AgentManager*                  manager);
```

The `blackboard`, `inbox`, `event_bus`, and `manager` are raw non-owning pointers. Their lifetimes must exceed the agent loop's lifetime — `AgentManager` guarantees this by joining the runner thread before destroying infrastructure.

---

### Queue

The queue is a `std::deque<unique_ptr<WorkItem>>` protected by `m_queue_mutex` + `m_queue_cv`.

#### `push`

```cpp
void push(std::unique_ptr<WorkItem> item, Position pos = Position::Back);
```

Appends or prepends to the queue from the agent's own loop thread. Does not acquire a lock (the agent loop is single-threaded on the queue side).

Actually uses the mutex — the queue is shared with external injection paths. `Position::Back` is the default (stages push new plan items to the back); `Position::Front` is used for high-priority injected items.

#### `injectFromOutside`

```cpp
void injectFromOutside(std::unique_ptr<WorkItem> item,
                       Position pos = Position::Front);
```

Thread-safe entry point for pushes from outside the agent thread (e.g. `AgentManager::injectWork`). Acquires the mutex, pushes, and notifies `m_queue_cv`.

#### `pop`

```cpp
std::unique_ptr<WorkItem> pop();
```

Blocks until an item is available or termination is signalled. Internally uses `wait_for(150ms)` (the **idle grace period**) rather than an infinite `wait`. If the queue is still empty after 150 ms of idling, returns `nullptr` to signal `QueueEmpty` termination.

The 150 ms bound ensures real-time injected items arrive before the timeout: items are injected during execution (not while the agent is idle), so the grace period only fires after work has fully drained.

#### `try_pop`

```cpp
std::unique_ptr<WorkItem> try_pop();
```

Non-blocking. Returns `nullptr` immediately if the queue is empty. Used by the `Agent` loop to drain all items pushed by the current stage in one round, so they form a single dependency-aware batch.

#### `wakeLoop`

```cpp
void wakeLoop();
```

Notifies `m_queue_cv` without pushing an item — used when setting `cancellation_flag` or `should_stop` so `pop()` wakes and observes the new state.

---

### History

```cpp
void recordResult(WorkResult result);
```

Appends to `m_history`. Called only from the agent loop after a batch completes — never from parallel worker threads. This is the deterministic merge point; results are always recorded in declared batch order.

```cpp
const WorkResult* lastResult()                      const;
const WorkResult* resultById(const std::string& id) const;
nlohmann::json    historySummaryJson(int max_entries = 20) const;
const std::vector<WorkResult>& history()            const;
```

`historySummaryJson` returns the N most recent entries as a compact JSON array suitable for injection into LLM prompts via the `{{HISTORY}}` placeholder.

---

### Reference Resolution

```cpp
nlohmann::json resolveReferences(const nlohmann::json& inputs) const;
```

Walks every string value in `inputs` and replaces tokens matching `^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$` with the corresponding value from history.

- `"$step1"` → `resultById("step1").output`
- `"$step1.field"` → `resultById("step1").output["field"]`

Throws `std::runtime_error` if a referenced id is not found in history. `BatchExecutor` calls this on the agent thread (before submitting to the pool) to satisfy the happens-before requirement — by the time a parallel worker sees the resolved value, the dependency result is fully written.

---

### Termination Flags

```cpp
std::atomic<bool> cancellation_flag{false};
bool              should_stop{false};
nlohmann::json    final_output;
int               iteration_count{0};
```

`cancellation_flag` is set externally (from any thread) via `AgentManager::cancelAgent`.  
`should_stop` is set by stages from within the loop (non-atomic; only the loop thread writes it).  
`final_output` carries structured output when `should_stop` is set.

---

### Other Accessors

```cpp
const AgentConfig& config()        const;
LLMClient&         llm();
WorkFactory&       factory();
PromptLoader&      promptLoader();
MemoryBackend&     memory();
Blackboard*        blackboard();
MessageInbox*      inbox();
EventBus*          eventBus();
AgentManager*      manager();
```

Plus the per-agent todo list:

```cpp
std::vector<std::string> todo_list;
```

Managed by `TodoWriteAction`. Not mutex-guarded — only accessible from the agent loop thread.

---

### Thread-Safety Summary

| Member | Thread-safe? | Guard |
|---|---|---|
| `m_queue` | Yes | `m_queue_mutex` + `m_queue_cv` |
| `m_history` | Yes (read) | `m_history_mutex` |
| `m_history` (write) | Loop thread only | (invariant) |
| `cancellation_flag` | Yes | `std::atomic` |
| `should_stop` | Loop thread only | (invariant) |
| `todo_list` | Loop thread only | (invariant) |

---

## Related Components

- [`AgentManager`](agent_manager.md) — spawns agents and manages their lifecycles
- [`BatchExecutor`](batch_executor.md) — DAG-based parallel execution of batches
- [`WorkItem`](work_item.md) — items queued and executed by the loop
- [`LLMClient`](llm_client.md) — LLM call interface
- [`PromptLoader`](prompt_loader.md) — template rendering
- [`Blackboard`](blackboard.md) — shared state (Pattern C)
- [`MessageInbox`](message_inbox.md) — per-agent messaging (Pattern B)
- [`EventBus`](event_bus.md) — event notifications
