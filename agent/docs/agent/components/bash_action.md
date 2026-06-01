# BashAction

`src/agent/actions/bash_action.hpp` · `src/agent/actions/bash_action.cpp`

## Overview

`BashAction` runs a shell command via `popen` and captures its stdout and exit code. A configurable timeout kills the command if it runs too long.

## Factory Registration

```
name:  "BashAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `command` | string | Yes | — | Shell command to execute |
| `timeout_ms` | integer | No | `10000` | Maximum execution time in milliseconds |

## Execution

1. Resolves `$ref` values in inputs.
2. Runs the command in a detached worker thread via `popen`.
3. Waits for completion using `std::future::wait_for(timeout_ms)`.
4. If the timeout fires, detaches the thread and returns `success=false` with error `"Command timed out after N ms"`.
5. On completion, calls `pclose` to get the exit code (extracted via `WEXITSTATUS` on POSIX).

## Output

| Field | Value |
|---|---|
| `success` | `true` when exit code is `0` |
| `output.stdout` | Captured stdout as a string |
| `output.exit_code` | Process exit code (`-1` on timeout) |
| `error` | `"Command exited with code N"` when non-zero |

## Example

```json
{
  "name": "BashAction",
  "id": "b1",
  "inputs": {
    "command": "ls -la /tmp",
    "timeout_ms": 5000
  }
}
```

Chained usage with `$ref`:

```json
[
  { "name": "BashAction", "id": "compile",
    "inputs": { "command": "cd /project && cmake --build ." }},
  { "name": "BashAction", "id": "test",
    "inputs": { "command": "echo Exit was $compile.exit_code" }}
]
```

## Notes

- Stderr is not captured by default. Redirect with `2>&1` in the command string.
- Large stdout (> 4 KB per `fgets` read) is accumulated in multiple reads.
- The worker thread is detached (leaked) on timeout; this is a known limitation.

## Related Components

- [`Action`](action.md) — base class
- [`AgentContext`](agent_context.md) — `resolveReferences` expands `$ref` in inputs
