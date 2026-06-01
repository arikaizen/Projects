# Blackboard

`include/agent/blackboard.hpp` · `src/agent/blackboard.cpp`

---

## Overview

`Blackboard` is a thread-safe shared key-value store accessible to all agents. It implements **Pattern C** — global shared state — and is the primary mechanism for agents to share intermediate results without direct inter-agent coupling.

One `Blackboard` instance lives inside `AgentManager`. Every agent's `AgentContext` holds a raw non-owning pointer to it.

---

## Construction

```cpp
explicit Blackboard(EventBus* event_bus = nullptr);
```

Optionally wired to an `EventBus`. When provided, a `blackboard_updated` event is emitted on every `write` call.

---

## API

### `write`

```cpp
void write(const std::string& key, nlohmann::json value);
```

Inserts or replaces the value under `key`. Emits `blackboard_updated` with `{"key": key}` if an event bus is configured.

### `read`

```cpp
std::optional<nlohmann::json> read(const std::string& key) const;
```

Returns the value under `key`, or `std::nullopt` if the key does not exist.

### `remove`

```cpp
void remove(const std::string& key);
```

Deletes the entry for `key`. No-op if the key is absent.

### `keys`

```cpp
std::vector<std::string> keys(const std::string& prefix = "") const;
```

Returns all keys that start with `prefix`, sorted lexicographically. Pass `""` to list all keys.

### `contains`

```cpp
bool contains(const std::string& key) const;
```

Returns `true` if the key exists.

---

## Thread-Safety

All methods acquire `m_mutex` (a `std::mutex`) exclusively. Concurrent `write` and `read` calls from different agent threads are safe.

For read-heavy workloads (many concurrent agents reading, few writing), upgrading to `std::shared_mutex` is a straightforward optimisation.

---

## Usage Pattern

The most common pattern in fan-out/fan-in scenarios:

```
// Writer agents (fan-out workers):
BlackboardWriteAction: key="research.legal",    value=$legal_summary
BlackboardWriteAction: key="research.technical", value=$tech_summary

// Synthesiser agent (fan-in):
BlackboardReadAction: key="research.legal"      → $legal_data
BlackboardReadAction: key="research.technical"  → $tech_data
TransformStage:       instruction="combine $legal_data and $tech_data"
```

`AgentManager` also exposes convenience wrappers:

```cpp
manager.blackboardWrite("key", value);
manager.blackboardRead("key");
manager.blackboardKeys("prefix.");
manager.blackboardDelete("key");
```

---

## Actions

Three built-in actions expose the blackboard to agent plans:

| Action | Operation |
|---|---|
| `BlackboardWriteAction` | `write(key, value)` |
| `BlackboardReadAction` | `read(key)` — fails if key absent |
| `BlackboardListAction` | `keys(prefix)` |

See [`actions.md`](actions.md) for input/output schemas.

---

## C ABI Exposure

```c
am_status_t am_blackboard_write(AgentManager* mgr, const char* key, const char* value_json);
am_status_t am_blackboard_read(AgentManager* mgr, const char* key, char* out, size_t out_size);
am_status_t am_blackboard_keys(AgentManager* mgr, const char* prefix, char* out, size_t out_size);
```

See [C ABI](c_api.md).

---

## Related Components

- [`AgentManager`](agent_manager.md) — owns the `Blackboard`; exposes convenience wrappers
- [`AgentContext`](agent.md) — exposes `blackboard()` pointer to actions
- [`EventBus`](event_bus.md) — receives `blackboard_updated` events
- [`Actions`](actions.md) — `Blackboard*Action` variants
- [`MessageInbox`](message_inbox.md) — per-agent alternative (Pattern B)
