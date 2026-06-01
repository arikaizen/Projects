# ThreadPool

`include/agent/thread_pool.hpp` · `src/agent/thread_pool.cpp`

## Overview

`ThreadPool` is a fixed-size worker thread pool used exclusively by `BatchExecutor` for intra-batch (L3) parallelism. Agent loops themselves run on dedicated `std::thread` instances managed by `AgentManager`, not on the pool, so pool threads are never blocked waiting for their own results.

## Construction

```cpp
explicit ThreadPool(std::size_t num_threads);
```

Spawns `num_threads` worker threads that block on a condition variable until work is available. The `AgentManager::Config::thread_pool_size` field controls this value (default: 16).

## `submit`

```cpp
template<typename F, typename... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>;
```

Wraps the callable and arguments in a `std::packaged_task`, pushes it to the internal queue, notifies one worker, and returns the associated `std::future`. Thread-safe.

Throws `std::runtime_error` if `shutdown()` has been called.

## `shutdown`

```cpp
void shutdown();
```

Sets the stop flag, wakes all workers, and joins them. Called by the destructor.

## `size`

```cpp
std::size_t size() const { return m_workers.size(); }
```

Returns the number of worker threads.

## Concurrency Role

| Level | What runs here |
|---|---|
| L1 | Per-user `QuotaManager` — limits per user |
| L2 | Dedicated `std::thread` — one per `Agent` loop |
| **L3** | **`ThreadPool`** — parallel action execution within a batch |
| L4 | Fan-out futures — `AgentManager::fanOut` |

The pool is intentionally not used for agent loops. If agent loops ran on pool threads and submitted new work to the same pool, they could deadlock (waiting for futures of work that the pool has no capacity to start).

## Related Components

- [`BatchExecutor`](batch_executor.md) — the sole consumer of this pool
- [`AgentManager`](agent_manager.md) — owns the pool; passes it to each `Agent`
