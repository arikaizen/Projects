# LocateStage

`src/agent/stages/locate_stage.hpp` · `src/agent/stages/locate_stage.cpp`

## Overview

`LocateStage` is **Step 2B** of the six-phase agent loop. It asks the LLM which specific resources need to be found (files, symbols, memory entries) and pushes the corresponding search/locate actions (GlobAction, GrepAction, MemoryReadAction, etc.). It then pushes a `ReadStage` that declares `$ref` dependencies on all locate action IDs, ensuring `ReadStage` runs only after every locate action has completed.

## Factory Registration

```
name: "LocateStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override for the agent's default task |

## Execution

1. Reads `task`, `"agent:understanding"`, and `"agent:orientation"` from blackboard.
2. Renders `locate_stage.md` with `{{TASK}}`, `{{UNDERSTANDING}}`, `{{ORIENTATION}}`, `{{OUTPUT_SCHEMA}}`.
3. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=2048})`.
4. Parses the `{"actions": [...]}` response.
5. Validates each action (registered type, unique ID) and pushes it to the queue.
6. Pushes `ReadStage` (id: `"auto_read"`) with `inputs["locate_results"]` containing a `$ref` to every locate action's output, creating BatchExecutor DAG dependencies.

## LLM Response Format

```json
{
  "actions": [
    {
      "name": "GlobAction",
      "id":   "loc_headers",
      "inputs": { "pattern": "include/**/*.hpp" }
    },
    {
      "name": "GrepAction",
      "id":   "loc_usage",
      "inputs": { "pattern": "AgentManager", "paths": ["src/"] }
    }
  ]
}
```

## DAG Wiring

`ReadStage` inputs contain:

```json
{ "locate_results": { "loc_headers": "$loc_headers", "loc_usage": "$loc_usage" } }
```

The `$ref` strings declare DAG dependencies. `BatchExecutor` ensures `ReadStage` does not start until both `loc_headers` and `loc_usage` have completed. When `ReadStage.execute()` runs, `inputs["locate_results"]` holds the actual resolved outputs.

## Related Components

- [`OrientStage`](orient_stage.md) — previous stage (Step 2A)
- [`ReadStage`](read_stage.md) — next stage (Step 2C), pushed with $ref deps
- [`BatchExecutor`](batch_executor.md) — DAG dependency resolution
- [`stages.md`](stages.md) — six-phase overview
