# GrepAction

`src/agent/actions/grep_action.hpp` · `src/agent/actions/grep_action.cpp`

## Overview

`GrepAction` searches file contents for a pattern. It uses **ripgrep** (`rg`) if available on `$PATH`, otherwise falls back to a built-in C++ implementation using `std::regex` or substring search. Works on both a single file and a directory (recursive).

## Factory Registration

```
name:  "GrepAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `pattern` | string | Yes | — | Search pattern |
| `path` | string | No | `"."` | File or directory to search |
| `regex` | boolean | No | `false` | Treat pattern as a regular expression |

## Execution

1. Resolves `$ref` values in inputs.
2. Detects ripgrep availability by running `rg --version 2>/dev/null`.
3. If rg is available: runs `rg --line-number --with-filename --no-heading [--fixed-strings] PATTERN PATH`.
4. If rg is not available: iterates files with `std::filesystem::recursive_directory_iterator`; for each file, reads line-by-line and applies `std::regex_search` (if `regex=true`) or `std::string::find` (if `regex=false`).

## Output

| Field | Value |
|---|---|
| `success` | `true` (zero matches is not an error) |
| `output.matches` | JSON array of match objects |
| `output.count` | Total number of matches |

Each match object:

```json
{ "file": "/path/to/file.cpp", "line_number": 42, "line": "the matched line text" }
```

## Example

```json
{ "name": "GrepAction", "id": "gr1",
  "inputs": { "pattern": "TODO", "path": "/project/src" } }
```

Regex search:

```json
{ "name": "GrepAction", "id": "gr2",
  "inputs": {
    "pattern": "^class [A-Z][a-zA-Z]+",
    "path": "/project/include",
    "regex": true
  }
}
```

## Notes

- The ripgrep backend is significantly faster for large codebases.
- Invalid regex patterns (when `regex=true`) silently fall back to literal string search.
- Binary files may produce garbled output with the manual fallback; ripgrep skips them automatically.

## Related Components

- [`Action`](action.md) — base class
- [`GlobAction`](glob_action.md) — searches by filename pattern rather than content
- [`BashAction`](bash_action.md) — can run arbitrary `grep`/`rg` commands directly
