# BatchExecutor

`include/agent/batch_executor.hpp` · `src/agent/batch_executor.cpp`

---

## Overview

`BatchExecutor` executes a collection of `WorkItem`s with **dependency-aware parallelism** (Level 3 concurrency). Given a batch, it builds a DAG from `$ref` dependency declarations, validates acyclicity, and runs all items whose dependencies are already satisfied concurrently on the shared `ThreadPool`. As items complete, newly unblocked items are dispatched. Results are merged into `AgentContext::history` in the batch's **declared order** after all items finish.

One `BatchExecutor` instance lives inside `Agent`. It is created fresh per `Agent` and is not shared across threads.

---

## Algorithm

### 1. DAG Construction (`buildDAG`)

For each `WorkItem` in the batch:

1. Call `WorkItem::dependencies()` to get the set of referenced ids (all `$id` or `$id.field` patterns in `inputs`).
2. For each dependency:
   - If the id is already in `AgentContext::history` → satisfied immediately, no edge needed.
   - If the id belongs to another item in the *same batch* → add an edge; this item will wait.
   - If the id is unknown → treat as unsatisfied; will cause a skipped result at runtime.
3. Apply **Kahn's algorithm** to detect cycles. If a cycle is found, `buildDAG` returns `false` and populates `error_out`. `execute` throws `std::runtime_error`.

### 2. Reference Resolution

Before dispatching any item to the pool, `BatchExecutor` calls `ctx.resolveReferences(item->inputs)` on the **agent thread**. This satisfies the happens-before requirement: by the time a pool worker reads the resolved value, the dependency result is fully committed to history (or to the local results array, which uses a mutex for pool-side writes).

### 3. Execution

```
ready = {items with no pending in-batch deps}
while items_remaining:
    for each item in ready:
        submit to ThreadPool  (or run inline — see below)
    wait for any completion
    record result
    unlock dependents whose last dep just finished
    ready = newly unblocked items
```

### 4. Inline Fast-Path (Starvation Avoidance)

When a round has exactly **one ready item**, `BatchExecutor` runs it **inline on the calling (agent) thread** instead of submitting to the pool.

This prevents a subtle deadlock: if the agent loop itself occupies a pool thread, and it submits work that needs another pool thread, the system can stall with all threads occupied by agent loops and no thread free to execute batch items.

With the inline fast-path, single-item rounds never touch the pool — only true multi-item parallel rounds do. Combined with `AgentManager`'s policy of running each agent loop on a dedicated `std::thread` (not a pool thread), pool starvation is completely eliminated.

### 5. Failure Semantics

When an item fails:
- Its `WorkResult.success = false` and `WorkResult.error` is set.
- All items that **transitively depend** on the failed item are marked `skipped` with `skipped_reason = "dependency <id> failed"`.
- Items with no path to the failed item continue running normally.

### 6. Cancellation

Before starting each item, `BatchExecutor` checks `ctx.cancellation_flag`. Once set, no new items are started; in-flight items are allowed to complete naturally.

### 7. Deterministic Merge

After the batch completes (all futures collected, inline calls done), results are folded into `ctx.recordResult()` in the batch's **declared order**, not in completion order. This makes history deterministic regardless of execution timing.

---

## API

```cpp
explicit BatchExecutor(ThreadPool& pool);

std::vector<WorkResult> execute(
    std::vector<std::unique_ptr<WorkItem>> batch,
    AgentContext& ctx);
```

`execute` takes ownership of the batch items, runs them, records results into `ctx`, and returns the results vector (in declared order). Throws `std::runtime_error` if a dependency cycle is detected.

---

## Internal `Node` Structure

```cpp
struct Node {
    WorkItem*              item{nullptr};
    std::set<std::string>  pending_deps;   // in-batch deps not yet done
    std::vector<size_t>    dependents;     // indices of nodes waiting on this
    bool                   done{false};
    bool                   skipped{false};
    std::string            skip_reason;
};
```

`pending_deps` is decremented as dependencies complete. When it reaches zero, the node is added to the ready set.

---

## Thread-Safety Notes

- `BatchExecutor` itself is single-threaded (one instance per `Agent`, called from the agent thread).
- Pool workers write results into a shared `results[]` array protected by a local mutex inside `execute`.
- `ctx.recordResult()` is called only after all futures are collected — never from pool threads.

---

## Example

Given this batch:

```json
[
  {"id": "fetch",   "name": "WebFetchAction", "inputs": {"url": "https://example.com"}},
  {"id": "parse",   "name": "BashAction",     "inputs": {"command": "..."}},
  {"id": "report",  "name": "TransformStage", "inputs": {"text": "$fetch", "instruction": "summarise"}}
]
```

- `fetch` and `parse` have no in-batch deps → both dispatched concurrently in round 1.
- `report` depends on `$fetch` → waits until `fetch` completes.
- Once `fetch` finishes, `report` is unlocked and dispatched (inline if `parse` is still in-flight, or to the pool if `parse` is already done).
- Results are recorded in declared order: `fetch`, `parse`, `report`.

---

## Related Components

- [`Agent`](agent.md) — owns `BatchExecutor`; calls `execute` each loop iteration
- [`AgentContext`](agent.md) — supplies history for dep resolution and receives results
- [`ThreadPool`](thread_pool.md) — executes parallel items
- [`WorkItem`](work_item.md) — declares dependencies via `$ref` in inputs
