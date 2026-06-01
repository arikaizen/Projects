# Blackboard Actions

`src/agent/actions/blackboard_actions.hpp` · `src/agent/actions/blackboard_actions.cpp`
**Kind:** Action · **Pattern:** C (shared key-value state)
Registered by `registerBlackboardActions(factory)`.

Three actions expose the shared [`Blackboard`](blackboard.md) to agent plans.

---

## BlackboardWriteAction

**Factory name:** `BlackboardWriteAction`

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `key` | string | **yes** | Blackboard key |
| `value` | any JSON | **yes** | Value to store |

### Output

```json
{"key": "research.summary", "written": true}
```

---

## BlackboardReadAction

**Factory name:** `BlackboardReadAction`

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `key` | string | **yes** | Key to read |

### Output

```json
{"key": "research.summary", "value": {...}, "found": true}
```
When the key is absent: `{"key": "...", "found": false}`.

---

## BlackboardListAction

**Factory name:** `BlackboardListAction`

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `prefix` | string | no | Optional key-prefix filter |

### Output

```json
{"keys": ["research.summary", "research.sources"]}
```

---

## Thread-Safety

All three delegate to `Blackboard`, whose operations hold an internal mutex — safe under concurrent agent access.

## Related

- [Actions overview](actions.md) · [Blackboard](blackboard.md) — the store
- [AgentManager](agent_manager.md) — `blackboardWrite/Read/Keys/Delete`
- [Messaging actions](messaging_actions.md) — Pattern B alternative
