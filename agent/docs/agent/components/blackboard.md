# Blackboard

`include/agent/blackboard.hpp` · `src/agent/blackboard.cpp`

## Overview

`Blackboard` is a thread-safe, shared key-value store accessible to all agents spawned under the same `AgentManager`. Values are arbitrary JSON (`nlohmann::json`). It implements **Pattern C** — shared state coordination between agents.

## Construction

```cpp
explicit Blackboard(EventBus* event_bus = nullptr);
```

`event_bus` is optional. When provided, a `blackboard_updated` event is emitted on every `write` and `remove`.

## Interface

```cpp
void write(const std::string& key, nlohmann::json value);
std::optional<nlohmann::json> read(const std::string& key) const;
void remove(const std::string& key);
std::vector<std::string> keys(const std::string& prefix = "") const;
bool contains(const std::string& key) const;
```

### `write`

Inserts or updates the value under `key`. Emits `blackboard_updated` with `{"key": key, "action": "write"}` if an event bus is attached.

### `read`

Returns `std::optional<nlohmann::json>` — `std::nullopt` if the key does not exist. Never throws on missing key.

### `remove`

Deletes the key. Emits `blackboard_updated` with `{"key": key, "action": "remove"}` if an event bus is attached. No-op if the key does not exist.

### `keys`

Returns a sorted vector of all keys that start with `prefix`. Pass an empty string to list all keys.

### `contains`

Returns `true` if `key` exists.

## Thread-Safety

All methods acquire `m_mutex` for the duration of the operation. Concurrent reads and writes are serialised.

## Accessing from Actions

Agents access the blackboard via `ctx.blackboard()`, which returns a non-owning pointer to the shared instance. The blackboard actions (`BlackboardWriteAction`, `BlackboardReadAction`, `BlackboardListAction`) wrap these calls for use from LLM-generated plans.

## Related Components

- [`BlackboardWriteAction`](blackboard_actions.md), [`BlackboardReadAction`](blackboard_actions.md), [`BlackboardListAction`](blackboard_actions.md) — action-layer wrappers
- [`EventBus`](event_bus.md) — receives `blackboard_updated` events
- [`AgentManager`](agent_manager.md) — owns the shared blackboard; exposes `blackboardWrite/Read/Keys/Delete`
- [`AgentContext`](agent_context.md) — `ctx.blackboard()` non-owning pointer
