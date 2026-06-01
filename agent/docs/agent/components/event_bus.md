# EventBus

`include/agent/event_bus.hpp` · `src/agent/event_bus.cpp`

## Overview

`EventBus` is a simple synchronous publish-subscribe bus. Callbacks fire on the calling (engine) thread synchronously with the `emit` call. The mutex is **not** held during dispatch, so subscribers may re-enter the `AgentManager` (e.g. to call `cancelAgent`).

## Interface

```cpp
using EventCallback = std::function<void(const nlohmann::json& event)>;

void subscribe(EventCallback cb, void* key = nullptr);
void unsubscribe(void* key);
void emit(nlohmann::json event);
static nlohmann::json makeEvent(const std::string& type, nlohmann::json extra = {});
```

### `subscribe`

Registers a callback. `key` is an opaque pointer used for unsubscription (typically `this` or the address of the callback). Null keys are allowed but can only be unsubscribed by removing all null-key subscribers.

### `unsubscribe`

Removes the subscriber whose `key` matches. Safe to call from within a callback (the dispatch loop works from a snapshot).

### `emit`

1. Takes a snapshot of current subscribers under the mutex.
2. Releases the mutex.
3. Iterates the snapshot and calls each callback.

This snapshot-then-dispatch pattern means new subscriptions added during dispatch take effect on the next `emit`, and unsubscriptions during dispatch are safe.

### `makeEvent`

```cpp
static nlohmann::json makeEvent(const std::string& type, nlohmann::json extra = {});
```

Builds a typed event envelope with at minimum `{"type": "...", "timestamp": "<ISO8601>"}`. Additional fields from `extra` are merged in. All engine components use this when emitting events.

## Event Types

| Event type | Emitted by |
|---|---|
| `stage_start` | All stages |
| `stage_done` | All stages (on success) |
| `stage_error` | All stages (on failure) |
| `agent_final_answer` | Stages when `final_answer` is set |
| `validation_result` | `ValidateStage` |
| `corrective_injection` | `ValidateStage` |
| `blackboard_updated` | `Blackboard` |
| `agent_spawned`, `agent_started`, `agent_finished`, `agent_failed`, `agent_cancelled`, `agent_destroyed` | `AgentManager` |
| `work_item_started`, `work_item_finished` | `BatchExecutor` |
| `batch_started`, `batch_finished` | `BatchExecutor` |
| `work_injected` | `AgentManager::injectWork` |
| `message_sent`, `message_received` | `AgentManager` |
| `mcp_connected`, `mcp_disconnected` | `AgentManager` |
| `prompts_reloaded` | `AgentManager` |
| `quota_exceeded` | `QuotaManager` |

## Thread-Safety

`m_subs` is protected by `m_mutex`. The mutex is held only for the snapshot copy, not during dispatch. Dispatch itself is lock-free.

## Warning

Callbacks fire on engine threads. GUI consumers **must marshal to their UI thread** before touching any UI object.

## Related Components

- [`AgentManager`](agent_manager.md) — subscribes via `subscribeEvents`; emits lifecycle events
- [`Blackboard`](blackboard.md) — emits `blackboard_updated`
- [`Stages`](stages.md) — emit stage lifecycle events
- [`C ABI`](c_api.md) — `am_subscribe_events`, `am_unsubscribe_events`
