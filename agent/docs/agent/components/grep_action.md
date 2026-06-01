# GrepAction

`src/agent/actions/grep_action.hpp` · `src/agent/actions/grep_action.cpp`
**Factory name:** `GrepAction` · **Kind:** Action

---

## Purpose

Searches file contents for a pattern. Prefers `ripgrep` (`rg`) for speed and falls back to a manual `std::ifstream` / `std::regex` search when `rg` is not on `PATH`.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `pattern` | string | **yes** | Search pattern |
| `path` | string | no (default `"."`) | File or directory to search |
| `regex` | boolean | no (default `false`) | Treat `pattern` as a regular expression |

## Output

```json
{"matches": [{"file": "src/foo.cpp", "line": 42, "text": "..."}], "count": 1}
```

## Implementation Notes

- `ripgrepAvailable()` checks for `rg` on `PATH`.
- `runRipgrep(...)` shells out and parses results; `manualGrep(...)` is the pure-C++ fallback.

## Thread-Safety

Read-only; fully concurrent.

## Related

- [Actions overview](actions.md) · [GlobAction](glob_action.md) · [ReadAction](read_action.md) · [WorkItem](work_item.md)
