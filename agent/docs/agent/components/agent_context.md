# AgentContext

`include/agent/agent_context.hpp` · `src/agent/agent_context.cpp`

## Overview

`AgentContext` is the complete per-agent runtime state. It decouples the agent loop from `AgentManager` on the hot path: everything the loop needs is carried here directly — queue, history, LLM client, factory, prompt loader, blackboard pointer, inbox pointer, event bus pointer, and config.

## Configuration

```cpp
struct AgentConfig {
    std::string    agent_id;
    std::string    name;
    std::string    task;
    int            max_iterations{100};
    int            max_depth{3};
    int            current_depth{0};
    nlohmann::json extra;
};
```

| Field | Meaning |
|---|---|
| `agent_id` | Unique identifier assigned by `AgentManager` |
| `name` | Human-readable label |
| `task` | Initial task description injected into the first `ReasonStage` prompt |
| `max_iterations` | Loop iteration cap; triggers `MaxIterations` termination |
| `max_depth` | Sub-agent nesting limit enforced by `TaskAction` |
| `current_depth` | Current nesting depth (incremented by `TaskAction`) |
| `extra` | Arbitrary JSON passed through to prompts and tools |

## Construction

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

Raw pointer arguments (`blackboard`, `inbox`, `event_bus`, `manager`) are non-owning. Their lifetimes must exceed the agent loop's lifetime. `AgentManager` guarantees this by joining the runner thread before destroying any infrastructure.

---

## Queue

The queue is a `std::deque<unique_ptr<WorkItem>>` protected by `m_queue_mutex` and `m_queue_cv`.

### `push`

```cpp
void push(std::unique_ptr<WorkItem> item, Position pos = Position::Back);
```

Appends (`Position::Back`) or prepends (`Position::Front`) to the queue. Called from within the agent loop (stages push their planned items here). Also acquires the mutex to support the external injection path.

### `injectFromOutside`

```cpp
void injectFromOutside(std::unique_ptr<WorkItem> item,
                       Position pos = Position::Front);
```

Thread-safe entry point for pushes from outside the agent thread. Acquires the mutex, pushes, and notifies `m_queue_cv`. Called by `AgentManager::injectWork`.

### `pop`

```cpp
std::unique_ptr<WorkItem> pop();
```

Blocks until an item is available or termination is signalled. Uses `wait_for(150ms)` rather than an infinite wait. If the queue is still empty after 150 ms of idling, returns `nullptr` to signal `QueueEmpty` termination.

The 150 ms grace period is intentional: items injected while the agent is busy arrive before the idle timer starts, so the grace period only fires when work has fully drained.

### `try_pop`

```cpp
std::unique_ptr<WorkItem> try_pop();
```

Non-blocking. Returns `nullptr` immediately if the queue is empty. The `Agent` loop calls this after the blocking `pop()` to drain all items pushed by the current stage, forming a single dependency-aware batch.

### `wakeLoop`

```cpp
void wakeLoop();
```

Notifies `m_queue_cv` without pushing an item. Used when setting `cancellation_flag` or `should_stop` so `pop()` wakes and observes the new state.

### Other queue helpers

```cpp
bool   queueEmpty()  const;
size_t queueSize()   const;
nlohmann::json queueSummaryJson() const;
```

---

## History

```cpp
void recordResult(WorkResult result);
```

Appends to `m_history`. Called only from the agent loop after each batch completes — never from parallel worker threads. This is the deterministic merge point; results are always recorded in declared batch order regardless of parallel execution order.

```cpp
const WorkResult* lastResult()              const;
const WorkResult* resultById(const std::string& id) const;
nlohmann::json    historySummaryJson(int max_entries = 20) const;
const std::vector<WorkResult>& history()    const;
```

`historySummaryJson` returns the N most recent results as a compact JSON array for injection into LLM prompts via the `{{HISTORY}}` placeholder.

---

## Reference Resolution

```cpp
nlohmann::json resolveReferences(const nlohmann::json& inputs) const;
```

Walks every string value in `inputs` and replaces tokens matching `^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$` with values from history:

| Pattern | Resolves to |
|---|---|
| `"$step1"` | `resultById("step1").output` |
| `"$step1.field"` | `resultById("step1").output["field"]` |

Throws `std::runtime_error` if a referenced id is not found in history. `BatchExecutor` calls this on the agent thread before submitting to the pool, satisfying the happens-before requirement.

---

## ID Uniqueness

```cpp
bool idExists(const std::string& id) const;
```

Returns `true` if `id` is already present in history. Used by `ReasonStage` and `InjectionStage` during plan validation to reject duplicate ids before they are pushed to the queue.

---

## Termination Flags

```cpp
std::atomic<bool> cancellation_flag{false};
bool              should_stop{false};
nlohmann::json    final_output;
int               iteration_count{0};
```

| Flag | Set by | Meaning |
|---|---|---|
| `cancellation_flag` | Any thread via `AgentManager::cancelAgent` | Stops the loop after the current batch |
| `should_stop` | Stages from within the loop | Normal completion with optional structured output |
| `final_output` | Stages alongside `should_stop` | Carries the agent's answer |
| `iteration_count` | The loop itself | Incremented each iteration; checked against `max_iterations` |

---

## Accessors

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

---

## Per-Agent Todo List

```cpp
std::vector<std::string> todo_list;
```

Managed by `TodoWriteAction`. Not mutex-guarded — only accessible from the agent loop thread.

---

## Thread-Safety Summary

| Member | Thread-safe? | Guard |
|---|---|---|
| `m_queue` | Yes | `m_queue_mutex` + `m_queue_cv` |
| `m_history` (read) | Yes | `m_history_mutex` |
| `m_history` (write) | Loop thread only | invariant |
| `cancellation_flag` | Yes | `std::atomic` |
| `should_stop` | Loop thread only | invariant |
| `todo_list` | Loop thread only | invariant |

---

## Related Components

- [`Agent`](agent.md) — drives the loop using this state
- [`BatchExecutor`](batch_executor.md) — executes batches; reads history for `$ref` resolution
- [`WorkItem`](work_item.md) — items queued and dequeued here
- [`LLMClient`](llm_client.md) — accessed via `ctx.llm()`
- [`MemoryBackend`](memory_backend.md) — accessed via `ctx.memory()`
- [`Blackboard`](blackboard.md) — accessed via `ctx.blackboard()`
- [`MessageInbox`](message_inbox.md) — accessed via `ctx.inbox()`
