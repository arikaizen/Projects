# Agent

`include/agent/agent.hpp` · `src/agent/agent.cpp`

## Overview

`Agent` owns an `AgentContext` and runs the event loop. It drives a single task from an initial state to a terminal state by repeatedly draining the queue, executing batches through `BatchExecutor`, and recording results into history.

One `Agent` exists per active task. Its `run()` method executes on a **dedicated `std::thread`** managed by `AgentManager`, keeping the shared `ThreadPool` free for intra-batch (L3) parallelism.

## The Six-Phase Loop

The agent follows a structured six-phase cycle driven by the stage chain. Each phase is a registered `Stage` type that the previous phase pushes onto the queue:

```
Step 1  UnderstandStage   — parse task into structured goal
Step 2A OrientStage       — survey tools, history, context
Step 2B LocateStage       — identify and execute resource searches
Step 2C ReadStage         — synthesise locate results
Step 2D ValidateStage     — verify gathered context (optional)
Step 2E CodeIntelStage    — analyse code structure (optional)
Step 3  ReasonStage       — LLM plans a batch of work items
Step 4  (BatchExecutor)   — parallel action execution
Step 5  ObserveStage      — inspect results, decide done or iterate
Step 6  RespondStage      — compose final answer, terminate
```

`ObserveStage` is wired with `$ref` dependencies on all plan item IDs by `ReasonStage`, so `BatchExecutor` guarantees it runs only after every action completes. If `ObserveStage` decides the task is not done, it pushes a new `ReasonStage` (back to step 3). The context-gathering phases (1–2) run only once at the start.

## Loop Mechanics

```
while not terminated:
    item  = ctx.pop()          // blocks; 150 ms idle grace before QueueEmpty
    batch = [item] + drain()   // try_pop() drains remainder without blocking
    results = BatchExecutor.execute(batch, ctx)
    ctx.history += results     // deterministic merge in declared order
    check termination conditions
```

Within a batch, actions with `$ref` cross-dependencies execute in Kahn-sorted order; independent actions run in parallel. `ObserveStage` always appears last in its batch because `ReasonStage` wires its `plan_results` input with `$ref` strings pointing to every other plan item.

## Construction

```cpp
explicit Agent(std::unique_ptr<AgentContext> ctx, ThreadPool& pool);
```

Takes ownership of the context. The pool is passed through to the `BatchExecutor`.

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
| `ShouldStop` | A stage set `AgentContext::should_stop` (e.g. `RespondStage` or `final_answer`) |
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

- [`AgentContext`](agent_context.md) — per-agent runtime state
- [`AgentManager`](agent_manager.md) — spawns agents and manages dedicated threads
- [`BatchExecutor`](batch_executor.md) — DAG-based parallel execution
- [`stages.md`](stages.md) — six-phase stage overview and chain diagram
- [`WorkItem`](work_item.md) — items in the queue
