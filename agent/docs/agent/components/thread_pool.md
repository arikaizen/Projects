# ThreadPool

`include/agent/thread_pool.hpp` · `src/agent/thread_pool.cpp`

---

## Overview

`ThreadPool` is a fixed-size pool of worker threads that processes submitted tasks concurrently. It is the Level 3 concurrency primitive in the agent engine: used exclusively by `BatchExecutor` for intra-batch parallelism.

**Critical design constraint:** agent loops are **never** submitted to the pool. Each `Agent` runs on its own dedicated `std::thread` managed by `AgentManager`. This separation prevents pool reentrancy deadlock — an agent loop that submits batch items to the pool can never stall because all pool threads are occupied by other agent loops.

---

## Construction

```cpp
explicit ThreadPool(std::size_t num_threads);
```

Spawns `num_threads` persistent worker threads that block on a condition variable until work is available. The default in `AgentManager::Config` is 16.

---

## `submit`

```cpp
template<typename F, typename... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>;
```

Wraps `f(args...)` in a `std::packaged_task`, enqueues it, and returns the associated `std::future`. The future resolves when a worker thread executes the task.

Throws `std::runtime_error("ThreadPool is stopped")` if called after `shutdown()`.

The callable and all arguments are moved into the task, so submit is safe with move-only types.

---

## `shutdown`

```cpp
void shutdown();
```

Sets the stop flag, wakes all workers, and joins them. Called by `~ThreadPool()`. Pending tasks that have not yet started are abandoned. Tasks already running complete normally.

---

## `size`

```cpp
std::size_t size() const;
```

Returns the number of worker threads.

---

## Internal Design

```
m_tasks: std::queue<std::function<void()>>
m_mutex: protects m_tasks and m_stop
m_cv:    wake workers when tasks arrive or stop is set
m_workers: std::vector<std::thread> (size = num_threads)
```

Each worker loop:
```
while true:
    lock m_mutex
    wait on m_cv until m_tasks.empty() == false OR m_stop
    if m_stop and m_tasks empty: return
    task = m_tasks.front(); m_tasks.pop()
    unlock
    task()
```

---

## Thread-Safety

All methods are thread-safe. `submit` may be called from any thread concurrently.

---

## TSan / Sanitizer Notes

The pool is verified clean under ThreadSanitizer (TSan). Build with:

```bash
cmake -DSANITIZER_FLAGS=-fsanitize=thread ..
make check-tsan
```

A separate `agent_core_tsan` static library is compiled with `-fsanitize=thread` so the sanitizer instruments the pool itself, not just tests.

---

## Concurrency Levels Reference

| Level | Mechanism |
|---|---|
| L1: User isolation | Per-user `QuotaManager` limits |
| L2: Agent loops | Dedicated `std::thread` per `Agent` (not pool threads) |
| **L3: Batch parallelism** | **`ThreadPool` — this component** |
| L4: Async fan-out | `AgentManager::fanOut` returning `std::vector<future>` |

---

## Related Components

- [`BatchExecutor`](batch_executor.md) — the sole consumer of `submit`
- [`AgentManager`](agent_manager.md) — owns the pool; configures `thread_pool_size`
- [Concurrency guide](../concurrency.md) — full explanation of all 4 levels
