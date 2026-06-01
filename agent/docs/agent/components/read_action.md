# ReadAction

`src/agent/actions/read_action.hpp` · `src/agent/actions/read_action.cpp`
**Factory name:** `ReadAction` · **Kind:** Action

---

## Purpose

Reads a file from disk, optionally selecting a range of lines.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | **yes** | Absolute or relative file path |
| `offset` | integer | no (default 1) | 1-based start line |
| `limit` | integer | no | Maximum number of lines to return |

`$ref` values are resolved before use.

## Output

```json
{"content": "<file text>", "path": "<path>", "lines": 42}
```
On error: `{"content": "", "path": "<path>", "lines": 0}` with `success=false`.

## Thread-Safety

Read-only; concurrent reads (even of the same file) are safe.

## Related

- [Actions overview](actions.md) · [WriteAction](write_action.md) · [EditAction](edit_action.md) · [WorkItem](work_item.md)
