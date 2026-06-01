# TodoWriteAction

`src/agent/actions/todo_write_action.hpp` · `src/agent/actions/todo_write_action.cpp`

## Overview

`TodoWriteAction` manages the per-agent todo list stored in `AgentContext::todo_list`. This is a simple `std::vector<std::string>` that lives for the lifetime of the agent run and is accessible only from the agent loop thread.

## Factory Registration

```
name:  "TodoWriteAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `operation` | string | Yes | One of: `"add"`, `"remove"`, `"clear"`, `"list"` |
| `item` | string | Required for `add`/`remove` | Todo item text |

## Operations

| Operation | Effect |
|---|---|
| `add` | Appends `item` to the todo list |
| `remove` | Removes the first occurrence of `item` from the list (no-op if not found) |
| `clear` | Removes all items |
| `list` | No mutation; returns current list |

## Output

All operations return:

| Field | Value |
|---|---|
| `success` | `true` on any valid operation |
| `output.todo_list` | Current state of the todo list after the operation |

## Example

```json
[
  { "name": "TodoWriteAction", "id": "t1",
    "inputs": { "operation": "add", "item": "Download dataset" } },
  { "name": "TodoWriteAction", "id": "t2",
    "inputs": { "operation": "add", "item": "Train model" } },
  { "name": "TodoWriteAction", "id": "t3",
    "inputs": { "operation": "list" } }
]
```

`t3.todo_list` resolves to `["Download dataset", "Train model"]`.

## Notes

- `todo_list` is private to the agent and is **not** shared via the Blackboard. Use `BlackboardWriteAction` if shared state is needed.
- The `ReasonStage` prompt can include the current todo list via `{{HISTORY}}` (which serialises the last N results, including todo operations).

## Related Components

- [`Action`](action.md) — base class
- [`AgentContext`](agent_context.md) — owns `todo_list`
- [`BlackboardWriteAction`](blackboard_actions.md) — for shared state across agents
