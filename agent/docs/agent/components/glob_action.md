# GlobAction

`src/agent/actions/glob_action.hpp` · `src/agent/actions/glob_action.cpp`

## Overview

`GlobAction` recursively searches a directory tree for files matching a wildcard pattern. It supports `*` (match any substring) and `?` (match any single character). Pattern matching is attempted against both the filename (basename) and the full relative path.

## Factory Registration

```
name:  "GlobAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `pattern` | string | Yes | — | Glob pattern (e.g. `"*.cpp"`, `"src/*.hpp"`) |
| `root` | string | No | `"."` | Root directory to search |

## Execution

1. Resolves `$ref` values in inputs.
2. Checks if `root` exists; returns empty matches if it does not.
3. Iterates the directory tree with `std::filesystem::recursive_directory_iterator` (skips permission-denied entries).
4. For each entry, tests the pattern against:
   - The basename (`entry.path().filename().string()`)
   - The full relative path from `root`
5. Either match counts as a hit; the absolute path is added to the results.

## Wildcard Matching

Implemented with a dynamic-programming approach. `*` matches any sequence (including empty); `?` matches any single character. Matching is case-sensitive on Linux/macOS, case-dependent on Windows.

## Output

| Field | Value |
|---|---|
| `success` | Always `true` (empty matches is not an error) |
| `output.matches` | JSON array of matching absolute paths |
| `output.count` | Number of matches |

## Example

```json
{ "name": "GlobAction", "id": "g1",
  "inputs": { "pattern": "*.cpp", "root": "/project/src" } }
```

Use results in a subsequent action:

```json
{ "name": "BashAction", "id": "b1",
  "inputs": { "command": "wc -l $g1.matches" } }
```

## Related Components

- [`Action`](action.md) — base class
- [`GrepAction`](grep_action.md) — searches file contents rather than filenames
