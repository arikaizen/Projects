# BatchExecutor

`include/agent/batch_executor.hpp` · `src/agent/batch_executor.cpp`

## Overview

`BatchExecutor` executes a batch of `WorkItem` objects with dependency-aware parallelism. It builds a DAG from `$ref` dependencies, verifies acyclicity using Kahn's algorithm, then runs items concurrently on the shared `ThreadPool` as their dependencies are satisfied.

It is created fresh per batch and is not shared across threads.

## Construction

```cpp
explicit BatchExecutor(ThreadPool& pool);
```

## `execute`

```cpp
std::vector<WorkResult> execute(
    std::vector<std::unique_ptr<WorkItem>> batch,
    AgentContext& ctx);
```

Executes the batch and records results into `ctx`. Returns results in the batch's **declared order** regardless of execution order. Throws `std::runtime_error` if the dependency graph contains a cycle.

## Algorithm (L3 Concurrency)

1. **DAG construction** — For each item, call `item.dependencies()` to get its `$ref` ids. Refs that already exist in `ctx.history` are immediately satisfied. Refs that point to other items in the same batch create an in-batch dependency edge.

2. **Cycle detection** — Kahn's algorithm: compute in-degree for each node, start with zero-in-degree nodes, peel the graph. If any nodes remain after the algorithm, a cycle exists and the batch is rejected.

3. **Execution** — Items with no pending dependencies are "ready". All ready items are submitted to the thread pool concurrently. As each completes:
   - Its result is stored.
   - Its dependents are inspected; those whose last pending dependency just finished become newly ready.
   - Newly ready items are submitted immediately.

4. **Failure semantics** — A failed item's transitive dependents are not started. They are recorded as skipped with `skipped_reason` set. Independent items continue unaffected.

5. **Cancellation** — `ctx.cancellation_flag` is checked before starting each item. Once set, no new items are started; already-running items complete normally.

6. **Deterministic merge** — After all items finish (or are skipped/cancelled), results are folded into `ctx.history` in the batch's declared order at a single synchronisation point on the agent thread. Parallel workers never mutate history directly.

## Fast Path

When the batch contains exactly one ready item (the common case for sequential plans), the item is executed inline without submitting to the pool. This avoids thread-pool overhead for single-item batches.

## Internal Node Structure

```cpp
struct Node {
    WorkItem*              item{nullptr};
    std::set<std::string>  pending_deps;   // in-batch deps not yet finished
    std::vector<size_t>    dependents;      // indices of nodes depending on this
    bool                   done{false};
    bool                   skipped{false};
    std::string            skip_reason;
};
```

## Stage vs Action Ordering

Stages and Actions both derive from `WorkItem` and live in the same batch vector. Within a batch, stages execute **before** actions because the `Agent` loop drains the queue in declaration order and stages push their planned actions — the stage runs first, pushes actions, the loop drains those actions into the same batch. The DAG then handles ordering among the actions.

## Related Components

- [`Agent`](agent.md) — calls `execute` on each batch
- [`AgentContext`](agent_context.md) — provides history for `$ref` resolution and records results
- [`ThreadPool`](thread_pool.md) — the pool used to run items in parallel
- [`WorkItem`](work_item.md) — `dependencies()` provides the DAG edges
