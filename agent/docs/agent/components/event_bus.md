# EventBus

`include/agent/event_bus.hpp` · `src/agent/event_bus.cpp`

---

## Overview

`EventBus` is a simple synchronous publish/subscribe bus. It decouples event producers (agent loops, `BatchExecutor`, `Blackboard`) from consumers (GUI panels, logging, monitoring). Callbacks fire on the engine thread that called `emit` — GUI consumers **must** marshal to their UI thread before touching UI objects.

One `EventBus` instance lives inside `AgentManager` and is shared by all agents.

---

## Subscription

```cpp
void subscribe(EventCallback cb, void* key = nullptr);
void unsubscribe(void* key);
```

`key` is any stable pointer that uniquely identifies the subscriber (e.g. `this`, a callback address, or a heap-allocated sentinel). Used for unsubscription.

`subscribe` acquires the mutex and appends to the subscriber list.  
`unsubscribe` acquires the mutex and removes the matching entry.

```cpp
// EventCallback type:
using EventCallback = std::function<void(const nlohmann::json& event)>;
```

---

## Emission

```cpp
void emit(nlohmann::json event);
```

Fires all registered callbacks synchronously. Importantly, the mutex is **not held during dispatch** — subscribers may safely call back into `AgentManager` (e.g. to inspect agent status) without deadlocking.

The dispatch snapshot is taken under the lock, then the lock is released before any callback is invoked.

---

## Event Construction

```cpp
static nlohmann::json makeEvent(const std::string& type, nlohmann::json extra = {});
```

Builds a standard event envelope:

```json
{
  "type":      "work_item_finished",
  "timestamp": "2026-06-01T12:00:00.123Z",
  ...extra fields...
}
```

Callers merge extra fields (e.g. `agent_id`, `duration_ms`) into the envelope.

---

## Event Types

| Type | Emitted by | Extra fields |
|---|---|---|
| `agent_spawned` | `AgentManager::spawnAgent` | `agent_id`, `user_id`, `name` |
| `agent_started` | `AgentManager::runAgent` | `agent_id`, `task` |
| `agent_finished` | `AgentManager::onAgentFinished` | `agent_id`, `result` |
| `agent_failed` | `AgentManager::onAgentFinished` | `agent_id`, `error` |
| `agent_cancelled` | `AgentManager::cancelAgent` | `agent_id` |
| `agent_destroyed` | `AgentManager::destroyAgent` | `agent_id` |
| `work_item_started` | `BatchExecutor` | `agent_id`, `item_id`, `item_name`, `ran_in_parallel` |
| `work_item_finished` | `BatchExecutor` | `agent_id`, `item_id`, `success`, `duration_ms`, `ran_in_parallel` |
| `batch_started` | `BatchExecutor` | `agent_id`, `batch_size` |
| `batch_finished` | `BatchExecutor` | `agent_id`, `batch_size`, `duration_ms` |
| `work_injected` | `AgentManager::injectWork` | `agent_id`, `item_name` |
| `message_sent` | `AgentManager::sendMessage` | `from_id`, `to_id` |
| `message_received` | `MessageInbox::push` | `to_id` |
| `blackboard_updated` | `Blackboard::write` | `key` |
| `mcp_connected` | `AgentManager::connectMCP` | `server_name`, `url` |
| `mcp_disconnected` | `AgentManager::disconnectMCP` | `server_name` |
| `mcp_notification` | `MCPToolAction` | `server_name`, `notification` |
| `prompts_reloaded` | `AgentManager::reloadPrompts` | `prompts_dir` |
| `quota_exceeded` | `AgentManager::spawnAgent` | `user_id`, `resource` |

---

## Thread-Safety

| Operation | Guard |
|---|---|
| `subscribe` / `unsubscribe` | `m_mutex` (exclusive) |
| `emit` — snapshot | `m_mutex` (exclusive, briefly) |
| `emit` — dispatch | No lock; re-entrant safe |

Callbacks run with no lock held, so they may safely call any `AgentManager` method.

---

## C ABI Exposure

```c
typedef void (*am_event_cb)(const char* event_json, void* user_data);
am_status_t am_subscribe_events(AgentManager* mgr, am_event_cb cb, void* user_data);
am_status_t am_unsubscribe_events(AgentManager* mgr, am_event_cb cb);
```

The C ABI wraps each callback in a C++ lambda that serialises the JSON event to a `const char*` string. `user_data` is threaded through to the C callback. See [C ABI](c_api.md).

---

## Related Components

- [`AgentManager`](agent_manager.md) — owns the bus; exposes `subscribeEvents` / `unsubscribeEvents`
- [`Blackboard`](blackboard.md) — emits `blackboard_updated` on write
- [`BatchExecutor`](batch_executor.md) — emits `work_item_*` and `batch_*` events
- [`AgentContext`](agent.md) — exposes `eventBus()` accessor for use from actions
