# Blackboard Actions

`src/agent/actions/blackboard_actions.hpp` · `src/agent/actions/blackboard_actions.cpp`

## Overview

The blackboard actions implement **Pattern C** (shared state). They provide read/write/list access to the `Blackboard` — a thread-safe, shared key-value store accessible by all agents under the same `AgentManager`. Three action types are provided.

---

## BlackboardWriteAction

Writes a JSON value under a key.

### Factory Registration

```
name:  "BlackboardWriteAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `key` | string | Yes | Blackboard key |
| `value` | any | Yes | Arbitrary JSON value to store |

### Output

| Field | Value |
|---|---|
| `success` | `true` if written |
| `output.key` | The key |
| `output.written` | `true` |

---

## BlackboardReadAction

Reads a value from the blackboard by key.

### Factory Registration

```
name:  "BlackboardReadAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `key` | string | Yes | Blackboard key to read |

### Output

| Field | Value |
|---|---|
| `success` | `true` if key exists, `false` if not found |
| `output.key` | The key |
| `output.value` | The stored JSON value (only when `found=true`) |
| `output.found` | `true` or `false` |
| `error` | `"Key not found: <key>"` when not found |

---

## BlackboardListAction

Lists all blackboard keys, optionally filtered by prefix.

### Factory Registration

```
name:  "BlackboardListAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `prefix` | string | No | Filter keys to those starting with this prefix |

### Output

| Field | Value |
|---|---|
| `success` | `true` |
| `output.keys` | JSON array of matching key strings |

---

## Pattern C Flow

```
Agent A                        Agent B
  ↓                              ↓
BlackboardWriteAction         BlackboardReadAction
  → Blackboard::write(k,v)   ← Blackboard::read(k)
        ↓                            ↑
    shared Blackboard  ─────────────
```

The blackboard emits a `blackboard_updated` event on `EventBus` after each write.

## Notes

- Blackboard access is available from any agent via `ctx.blackboard()`.
- These actions require `ctx.blackboard() != nullptr`, which is always true when an agent is spawned through `AgentManager`.

## Related Components

- [`Action`](action.md) — base class
- [`Blackboard`](blackboard.md) — the shared store these actions operate on
- [`AgentContext`](agent_context.md) — `ctx.blackboard()` accessor
