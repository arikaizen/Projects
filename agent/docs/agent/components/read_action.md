# ReadAction

`src/agent/actions/read_action.hpp` · `src/agent/actions/read_action.cpp`

## Overview

`ReadAction` reads a file from disk and returns its content. An optional line range can be specified with `offset` (1-based start line) and `limit` (maximum number of lines to return).

## Factory Registration

```
name:  "ReadAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `path` | string | Yes | — | Absolute or relative file path |
| `offset` | integer | No | `1` | 1-based start line; `0` or `1` both mean the first line |
| `limit` | integer | No | `-1` (no limit) | Maximum number of lines to return |

## Execution

1. Resolves `$ref` values in inputs.
2. Opens the file with `std::ifstream`.
3. If no `offset`/`limit` is needed, reads the entire file in one pass via `rdbuf`.
4. With `offset` or `limit`, reads line-by-line, skipping lines before the start and stopping after `limit` lines are collected. Remaining lines are still counted for the `lines` field.

## Output

| Field | Value |
|---|---|
| `success` | `true` if the file was opened and read |
| `output.content` | File content as a string (within the line range) |
| `output.path` | The requested path |
| `output.lines` | Total line count of the file |
| `error` | `"Cannot open file: <path>"` if the file cannot be opened |

## Example

```json
{ "name": "ReadAction", "id": "r1", "inputs": { "path": "/etc/hosts" } }
```

Read lines 10–20:

```json
{ "name": "ReadAction", "id": "r2",
  "inputs": { "path": "/var/log/syslog", "offset": 10, "limit": 11 } }
```

## Related Components

- [`Action`](action.md) — base class
- [`WriteAction`](write_action.md) — writes files
- [`EditAction`](edit_action.md) — edits files
