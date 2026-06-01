# WriteAction

`src/agent/actions/write_action.hpp` · `src/agent/actions/write_action.cpp`

## Overview

`WriteAction` writes a string to a file, truncating any existing content. Parent directories are created automatically if they do not exist.

## Factory Registration

```
name:  "WriteAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `path` | string | Yes | Destination file path (absolute or relative) |
| `content` | string | Yes | Text content to write |

## Execution

1. Resolves `$ref` values in inputs.
2. Creates any missing parent directories via `std::filesystem::create_directories`.
3. Opens the file with `std::ios::out | std::ios::trunc | std::ios::binary`.
4. Writes the full content string.

## Output

| Field | Value |
|---|---|
| `success` | `true` if the file was written without error |
| `output.path` | The written file path |
| `output.bytes_written` | Number of bytes written (`0` on failure) |
| `error` | `"Cannot open file for writing: <path>"` on failure |

## Example

```json
{
  "name": "WriteAction",
  "id": "w1",
  "inputs": {
    "path": "/tmp/output.txt",
    "content": "Hello, world!\n"
  }
}
```

Writing transformed content from a previous stage:

```json
{
  "name": "WriteAction",
  "id": "w2",
  "inputs": {
    "path": "/tmp/summary.md",
    "content": "$t1.transformed_text"
  }
}
```

## Notes

- `WriteAction` always truncates the file. To preserve existing content and make targeted edits, use [`EditAction`](edit_action.md).
- Two `WriteAction` items writing the same file must be ordered via `$ref` to avoid a data race.

## Related Components

- [`Action`](action.md) — base class
- [`ReadAction`](read_action.md) — reads files
- [`EditAction`](edit_action.md) — replaces a substring in an existing file
