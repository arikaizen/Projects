# Agent

`include/agent/agent.hpp` · `src/agent/agent.cpp`

## Overview

`Agent` owns an `AgentContext` and runs the event loop. It drives a single task from an initial state to a terminal state by repeatedly draining the queue, executing batches through `BatchExecutor`, and recording results into history.

One `Agent` exists per active task. Its `run()` method executes on a **dedicated `std::thread`** managed by `AgentManager`, keeping the shared `ThreadPool` free for intra-batch (L3) parallelism.

## Construction

```cpp
explicit Agent(std::unique_ptr<AgentContext> ctx, ThreadPool& pool);
```

Takes ownership of the context. The pool is passed through to the `BatchExecutor`.

## Loop Invariant

```
while not terminated:
    item  = ctx.pop()          // blocks; 150ms idle grace before QueueEmpty
    batch = [item] + drain()   // try_pop() drains remainder without blocking
    results = BatchExecutor.execute(batch, ctx)
    ctx.history += results     // deterministic merge in declared order
    check termination conditions
```

Stages within a batch execute sequentially: each sees the full prior history before the next one starts. Actions within a batch execute in parallel subject to `$ref` dependency ordering.

## `run()`

```cpp
RunResult run();
```

Blocking. Runs until one of the termination conditions fires, then returns `RunResult`.

## `RunResult`

```cpp
struct RunResult {
    TerminationReason reason;
    nlohmann::json    output;
    int               iterations{0};
    std::string       error;
};
```

| Field | Meaning |
|---|---|
| `reason` | Why the loop terminated |
| `output` | `AgentContext::final_output` when `reason == ShouldStop` |
| `iterations` | `AgentContext::iteration_count` at termination |
| `error` | Exception message when `reason == Error` |

## `TerminationReason`

| Value | Meaning |
|---|---|
| `QueueEmpty` | Queue drained and 150 ms idle grace period expired — normal completion |
| `ShouldStop` | A stage set `AgentContext::should_stop` (e.g. emitted a `final_answer`) |
| `MaxIterations` | `AgentConfig::max_iterations` was reached |
| `Cancelled` | `AgentContext::cancellation_flag` was set externally |
| `Error` | An unhandled exception escaped a work item |

## Context Access

```cpp
AgentContext&       context();
const AgentContext& context() const;
```

## Static Helper

```cpp
static std::string reasonToString(TerminationReason r);
```

Returns a human-readable string for logging.

## Related Components

- [`AgentContext`](agent_context.md) — the per-agent runtime state this class drives
- [`AgentManager`](agent_manager.md) — spawns agents and manages their dedicated threads
- [`BatchExecutor`](batch_executor.md) — executes each batch with DAG-based parallelism
- [`WorkItem`](work_item.md) — items in the queue
