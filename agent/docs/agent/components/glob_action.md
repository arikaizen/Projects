# GlobAction

`src/agent/actions/glob_action.hpp` · `src/agent/actions/glob_action.cpp`
**Factory name:** `GlobAction` · **Kind:** Action

---

## Purpose

Recursively searches a directory tree for paths matching a shell-style glob pattern. Supports `*` (any sequence within a path component) and `?` (any single character); no character classes.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `pattern` | string | **yes** | Glob pattern, e.g. `"*.cpp"` or `"src/*.hpp"` |
| `root` | string | no (default `"."`) | Root directory to search |

## Output

```json
{"matches": ["src/foo.cpp", "src/bar.cpp"], "count": 2}
```

## Implementation Note

Matching is performed by the private `wildcardMatch(pattern, name)` helper (handles `*` and `?`).

## Thread-Safety

Read-only filesystem traversal; fully concurrent.

## Related

- [Actions overview](actions.md) · [GrepAction](grep_action.md) · [ReadAction](read_action.md) · [WorkItem](work_item.md)
