# TodoWriteAction

`src/agent/actions/todo_write_action.hpp` · `src/agent/actions/todo_write_action.cpp`
**Factory name:** `TodoWriteAction` · **Kind:** Action

---

## Purpose

Manages the per-agent todo list stored in `AgentContext::todo_list`.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `operation` | string | **yes** | One of `add`, `remove`, `clear`, `list` |
| `item` | string | for `add`/`remove` | Todo item text |

## Operations

| Operation | Effect |
|---|---|
| `add` | Append `item` to the list |
| `remove` | Remove first occurrence of `item` |
| `clear` | Empty the list |
| `list` | Return the list unchanged |

## Output

```json
{"todo_list": ["Review PR #42", "Write tests"]}
```
(`clear` returns an empty array.)

## Thread-Safety

`todo_list` is only accessed from the agent's single loop thread — no concurrent access by design, so no mutex is needed.

## Related

- [Actions overview](actions.md) · [AgentContext](agent_context.md) — owns `todo_list` · [WorkItem](work_item.md)
