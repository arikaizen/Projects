# BashAction

`src/agent/actions/bash_action.hpp` · `src/agent/actions/bash_action.cpp`
**Factory name:** `BashAction` · **Kind:** Action

---

## Purpose

Runs a shell command via `popen()` and captures stdout and the exit code.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `command` | string | **yes** | Shell command to execute |
| `timeout_ms` | integer | no (default 10000) | Max execution time in milliseconds |

Inputs are passed through `resolveReferences`, so `command` may embed `$ref` values.

## Output

```json
{"stdout": "<captured output>", "exit_code": 0}
```
On failure to launch: `{"stdout": "", "exit_code": -1}` with `success=false`.

## Thread-Safety

`popen()` is thread-safe on glibc/Linux; concurrent `BashAction`s in a batch are safe there. On other libc implementations, serialize externally.

## Related

- [Actions overview](actions.md) · [WorkItem](work_item.md) · [AgentContext](agent_context.md)
