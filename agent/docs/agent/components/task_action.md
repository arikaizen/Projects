# TaskAction

`src/agent/actions/task_action.hpp` · `src/agent/actions/task_action.cpp`

## Overview

`TaskAction` implements **Pattern A** (delegation). It spawns a sub-agent via `AgentManager`, runs it synchronously with the given task, and returns its final output. This is the primary mechanism for hierarchical task decomposition.

## Factory Registration

```
name:  "TaskAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `task` | string | Yes | — | Task description for the sub-agent |
| `agent_name` | string | No | `"sub-agent"` | Human-readable name for the sub-agent |

## Execution

1. Resolves `$ref` values in inputs.
2. Checks that `ctx.manager()` is not null.
3. Checks that `ctx.config().current_depth < ctx.config().max_depth`; throws if the nesting limit is reached.
4. Builds an `AgentConfig` for the sub-agent, setting `current_depth = parent.current_depth + 1`.
5. Calls `ctx.manager()->spawnAgent(sub_config)` to create the sub-agent.
6. Calls `ctx.manager()->runAgentBlocking(sub_id, task)` — **blocks** until the sub-agent completes.
7. Calls `ctx.manager()->destroyAgent(sub_id)` to clean up.
8. Returns the sub-agent's final output as `output`.

## Output

| Field | Value |
|---|---|
| `success` | `true` when the sub-agent ran without exception |
| `output` | The sub-agent's `RunResult::output` JSON (typically `{"answer": "..."}`) |
| `error` | Exception message on depth exceeded or missing manager |

## Depth Enforcement

```
parent.current_depth (0) → sub-agent.current_depth (1) → ...
max_depth (default: 3)
```

When `current_depth >= max_depth`, the action throws `"Max agent depth exceeded"` and returns `success=false`. This prevents unbounded recursion.

## Example

```json
{
  "name": "TaskAction",
  "id": "sub1",
  "inputs": {
    "task": "Summarise the contents of /project/README.md",
    "agent_name": "summariser"
  }
}
```

## Notes

- `TaskAction` blocks the calling `BatchExecutor` slot for the duration of the sub-agent. For fan-out parallelism, use `AgentManager::fanOut` from C++ or `am_fan_out` from the C ABI instead.
- The sub-agent inherits `max_iterations` and `max_depth` from the parent.

## Related Components

- [`Action`](action.md) — base class
- [`AgentManager`](agent_manager.md) — `spawnAgent`, `runAgentBlocking`, `destroyAgent`
- [`AgentContext`](agent_context.md) — `manager()`, `config().current_depth`
