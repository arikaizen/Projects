# EditAction

`src/agent/actions/edit_action.hpp` · `src/agent/actions/edit_action.cpp`
**Factory name:** `EditAction` · **Kind:** Action

---

## Purpose

Performs an exact first-occurrence string replacement in a file.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | **yes** | File to edit |
| `old_string` | string | **yes** | Exact string to find and replace |
| `new_string` | string | **yes** | Replacement string |

`$ref` values are resolved before use.

## Output

```json
{"path": "<path>", "replaced": true}
```
`replaced` is `false` (with `success=false`) when the file is missing or `old_string` is not found.

## Thread-Safety

Concurrent edits to the **same file** are unsafe — serialize via `$ref` dependencies.

## Related

- [Actions overview](actions.md) · [ReadAction](read_action.md) · [WriteAction](write_action.md) · [WorkItem](work_item.md)
