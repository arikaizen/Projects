# WriteAction

`src/agent/actions/write_action.hpp` · `src/agent/actions/write_action.cpp`
**Factory name:** `WriteAction` · **Kind:** Action

---

## Purpose

Writes content to a file, creating parent directories as needed. Overwrites any existing content.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | **yes** | Destination file path |
| `content` | string | **yes** | Text content to write |

`$ref` values are resolved before use (e.g. write a previous result's output to disk).

## Output

```json
{"path": "<path>", "bytes_written": 1234}
```
On error: `{"path": "<path>", "bytes_written": 0}` with `success=false`.

## Thread-Safety

Concurrent writes to the **same path** are unsafe. Ensure only one `WriteAction` targets a given path at a time (serialize via `$ref` dependencies if needed).

## Related

- [Actions overview](actions.md) · [ReadAction](read_action.md) · [EditAction](edit_action.md) · [WorkItem](work_item.md)
