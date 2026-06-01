# EditAction

`src/agent/actions/edit_action.hpp` · `src/agent/actions/edit_action.cpp`

## Overview

`EditAction` performs an exact string replacement in a file. It finds the **first occurrence** of `old_string` in the file and replaces it with `new_string`. The file is read into memory, patched, and written back atomically.

## Factory Registration

```
name:  "EditAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `path` | string | Yes | File to edit |
| `old_string` | string | Yes | Exact string to find and replace |
| `new_string` | string | Yes | Replacement string |

## Execution

1. Resolves `$ref` values in inputs.
2. Opens and reads the entire file.
3. Searches for the first occurrence of `old_string` with `std::string::find`.
4. If found, calls `std::string::replace` at that position.
5. Writes the modified content back to the same path (truncating).

## Output

| Field | Value |
|---|---|
| `success` | `true` if the file was opened (even if `old_string` was not found) |
| `output.path` | The edited file path |
| `output.replaced` | `true` if the substitution was made, `false` if `old_string` was not found |
| `error` | Set (but `success=true`) when `old_string` is not found; `success=false` only on I/O errors |

## Notes

- Only the **first occurrence** is replaced. To replace all occurrences, chain multiple `EditAction` items.
- `old_string` not found sets `error` but does not set `success=false`. This is intentional — not finding the string is not necessarily a fatal error for an agent plan.
- Two `EditAction` items on the same file must be ordered via `$ref` dependencies to avoid races.

## Example

```json
{
  "name": "EditAction",
  "id": "e1",
  "inputs": {
    "path": "/project/config.yaml",
    "old_string": "debug: false",
    "new_string": "debug: true"
  }
}
```

## Related Components

- [`Action`](action.md) — base class
- [`ReadAction`](read_action.md) — reads the file before editing
- [`WriteAction`](write_action.md) — full file overwrite
