# AgentContext

`include/agent/agent_context.hpp` · `src/agent/agent_context.cpp`

---

## Overview

`AgentContext` is the complete per-agent runtime state. It decouples the agent loop from `AgentManager` on the hot path — everything the loop needs is carried here directly: the work queue, the result history, the LLM client, the work factory, the prompt loader, and non-owning pointers to the shared blackboard, inbox, event bus, and manager.

> The [`Agent`](agent.md) loop drives an `AgentContext`. This page documents the context; see [`Agent`](agent.md) for the loop itself.

---

## AgentConfig

```cpp
struct AgentConfig {
    std::string    agent_id;
    std::string    name;
    std::string    task;
    int            max_iterations{100};
    int            max_depth{3};
    int            current_depth{0};
    nlohmann::json extra;        // user-defined passthrough (carries user_id)
};
```

`extra` typically carries `user_id` for quota attribution.

---

## Construction

```cpp
AgentContext(AgentConfig config,
             std::shared_ptr<LLMClient> llm,
             std::shared_ptr<WorkFactory> factory,
             std::shared_ptr<PromptLoader> prompt_loader,
             std::shared_ptr<MemoryBackend> memory,
             Blackboard* blackboard,
             MessageInbox* inbox,
             EventBus* event_bus,
             AgentManager* manager);
```

The shared subsystems are `shared_ptr` (co-owned); `blackboard`, `inbox`, `event_bus`, and `manager` are **non-owning** raw pointers whose lifetimes must exceed the agent loop. `AgentManager` guarantees this by joining the runner thread before destroying infrastructure.

---

## Queue

A `std::deque<unique_ptr<WorkItem>>` guarded by `m_queue_mutex` + `m_queue_cv`.

| Method | Description |
|---|---|
| `push(item, pos)` | Enqueue from the loop thread; `Position::Front` or `Back` |
| `injectFromOutside(item, pos)` | Thread-safe enqueue from any thread; notifies the loop |
| `pop()` | Blocking pop with a **150 ms idle grace** (`wait_for`); returns `nullptr` to signal `QueueEmpty` |
| `try_pop()` | Non-blocking; drains items a stage pushed so they batch together |
| `queueEmpty()` / `queueSize()` / `queueSummaryJson()` | Inspection |
| `wakeLoop()` | Notify the CV without enqueuing (used on cancel/stop) |

### The idle grace period

`pop()` uses `wait_for(150ms)` rather than an infinite wait. If still empty after the grace period, it returns `nullptr` so idle agents terminate cleanly. Real-time injection happens *during* execution, so injected items always arrive before the timeout fires.

---

## History

| Method | Description |
|---|---|
| `recordResult(result)` | Append to history — called **only** from the loop thread after a batch (deterministic merge point) |
| `lastResult()` | Most recent `WorkResult*` (or `nullptr`) |
| `resultById(id)` | Lookup by item id |
| `historySummaryJson(max=20)` | Compact recent-history JSON for prompts |
| `history()` | Full `const std::vector<WorkResult>&` |

History is guarded by `m_history_mutex` for reads; writes occur only on the loop thread.

---

## Reference Resolution

```cpp
nlohmann::json resolveReferences(const nlohmann::json& inputs) const;
```

Replaces strings matching `^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$`:

| Token | Resolves to |
|---|---|
| `"$step1"` | `resultById("step1")->output` |
| `"$step1.field"` | `resultById("step1")->output["field"]` |

Throws `std::runtime_error` for an unresolvable reference. [`BatchExecutor`](batch_executor.md) calls this on the agent thread before pool submission, giving a happens-before guarantee for parallel workers.

---

## Termination Flags

```cpp
std::atomic<bool> cancellation_flag{false};  // set from any thread
bool              should_stop{false};        // set by stages on the loop thread
nlohmann::json    final_output;              // carries structured final answer
int               iteration_count{0};
```

---

## Accessors & Extras

```cpp
const AgentConfig& config();
LLMClient&    llm();      WorkFactory&  factory();   PromptLoader& promptLoader();
MemoryBackend& memory();  Blackboard*   blackboard(); MessageInbox* inbox();
EventBus*     eventBus();  AgentManager* manager();
bool          idExists(const std::string& id) const;

std::vector<std::string> todo_list;   // managed by TodoWriteAction (loop-thread only)
```

---

## Thread-Safety Summary

| Member | Safety |
|---|---|
| `m_queue` | mutex + CV (any thread) |
| `m_history` reads | `m_history_mutex` |
| `m_history` writes | loop thread only |
| `cancellation_flag` | `std::atomic` |
| `should_stop`, `todo_list` | loop thread only |

---

## Related

- [Agent](agent.md) — the loop that drives this context
- [AgentManager](agent_manager.md) — constructs contexts via `makeContext`
- [BatchExecutor](batch_executor.md) — consumes history, records results
- [WorkItem](work_item.md) · [WorkFactory](work_factory.md) · [LLMClient](llm_client.md)
