# ReasonStage

`src/agent/stages/reason_stage.hpp` · `src/agent/stages/reason_stage.cpp`

## Overview

`ReasonStage` is the primary reasoning step. It surveys the full agent state (task, history, queue, catalog of available items) and asks the LLM to produce a plan — an ordered JSON array of `WorkItem` descriptors. Each descriptor is validated and pushed onto the queue.

`ReasonStage` is typically the first item in any agent's queue. Other stages may push additional `ReasonStage` instances when the task requires multiple reasoning rounds.

## Factory Registration

```
name:  "ReasonStage"
kind:  Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override the agent's default task for this reasoning step |

## Execution

1. Gathers template variables: `CATALOG` (all registered item types), `HISTORY` (last 20 results), `QUEUE` (pending items), `TASK` (agent task or `inputs["task"]`), `OUTPUT_SCHEMA` (expected plan JSON schema).
2. Renders `reason_stage.md` via `PromptLoader`.
3. Calls `ctx.llm().complete({system_prompt, user_msg, json_mode=true, temperature=0.3, max_tokens=4096})`.
4. Parses the JSON response. Accepts either:
   - A JSON array of plan items (normal path).
   - A JSON object with a `"final_answer"` key (termination path).
5. Validates each plan item (see Validation below) and pushes them to the queue.

## Plan Item Format

```json
[
  {
    "name": "BashAction",
    "id": "b1",
    "inputs": { "command": "echo hello" }
  },
  {
    "name": "ReadAction",
    "id": "r1",
    "inputs": { "path": "$b1.stdout" }
  }
]
```

The optional `"final_answer"` field on the last item signals the agent to stop after executing that item:

```json
[
  { "name": "BashAction", "id": "last", "inputs": {"command": "echo done"},
    "final_answer": "Task complete." }
]
```

## Validation Rules

1. Every `name` must be registered in `WorkFactory`.
2. Every `id` must be unique across both history and the current plan.
3. Every `$ref` dependency must point to an id present in history or an earlier item in the same plan.

Plan validation uses the same logic as `InjectionStage`. If validation fails, the stage returns `success=false` with the validation error.

## Output

| Field | Value |
|---|---|
| `success` | `true` on successful LLM call and valid plan |
| `output.plan_size` | Number of items in the plan |
| `output.plan` | The raw plan array as returned by the LLM |

On termination path: `output` is `{"answer": "..."}` and `ctx.should_stop = true`.

## Events Emitted

| Event type | When |
|---|---|
| `stage_start` | Before the LLM call |
| `stage_done` | After successful execution |
| `stage_error` | On LLM failure or invalid plan |
| `agent_final_answer` | When a `final_answer` is received |

## Related Components

- [`Stage`](stage.md) / [`stages.md`](stages.md) — base class and overview
- [`InjectionStage`](injection_stage.md) — similar plan validation logic
- [`WorkFactory`](work_factory.md) — plan items are validated against registered types
- [`PromptLoader`](prompt_loader.md) — renders `reason_stage.md`
- [`LLMClient`](llm_client.md) — the LLM call
