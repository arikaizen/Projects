# ReasonStage

`src/agent/stages/reason_stage.hpp` · `src/agent/stages/reason_stage.cpp`

## Overview

`ReasonStage` is **Step 3** of the six-phase agent loop — Reason & Decide. It surveys the full agent state (task, history, queue, catalog, and any blackboard context written by the setup phases) and asks the LLM to produce a plan: an ordered JSON array of `WorkItem` descriptors. Each descriptor is validated and pushed onto the queue.

After pushing all plan items, `ReasonStage` automatically appends an `ObserveStage` (Step 5) with `$ref` dependencies on every plan item ID. This means `BatchExecutor` runs `ObserveStage` only after all plan actions have completed.

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

1. Gathers template variables: `CATALOG`, `HISTORY` (last 20 results), `QUEUE`, `TASK`, `OUTPUT_SCHEMA`.
2. Renders `reason_stage.md` via `PromptLoader`.
3. Calls `ctx.llm().complete({system_prompt, user_msg, json_mode=true, temperature=0.3, max_tokens=4096})`.
4. Parses the JSON response. Accepts either:
   - A JSON array of plan items (normal path).
   - A JSON object with a `"final_answer"` key (termination path — bypasses ObserveStage).
5. Validates each plan item and pushes them to the queue.
6. Pushes `ObserveStage` (id: `"observe_<this_id>"`) with `inputs["plan_results"]` containing a `$ref` array pointing to every pushed plan item.

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

The optional `"final_answer"` field on the last item signals the agent to stop after that item (no `ObserveStage` is pushed in this case):

```json
[
  { "name": "BashAction", "id": "last", "inputs": {"command": "echo done"},
    "final_answer": "Task complete." }
]
```

## ObserveStage Auto-Injection

After a non-final plan, `validateAndPushPlan` appends:

```json
{
  "name": "ObserveStage",
  "id":   "observe_<reason_id>",
  "inputs": {
    "plan_results": ["$b1", "$r1"]
  }
}
```

`BatchExecutor` resolves the `$ref` strings into DAG edges, ensuring `ObserveStage` waits for `b1` and `r1` before running.

## Validation Rules

1. Every `name` must be registered in `WorkFactory`.
2. Every `id` must be unique across both history and the current plan.
3. Every `$ref` dependency must point to an id present in history or an earlier item in the same plan.

## Output

| Field | Value |
|---|---|
| `success` | `true` on successful LLM call and valid plan |
| `output.plan_size` | Number of items in the plan |
| `output.plan` | The raw plan array as returned by the LLM |

On termination path: `output` is `{"answer": "..."}` and `ctx.should_stop = true`.

## Related Components

- [`ObserveStage`](observe_stage.md) — auto-appended after every non-final plan (Step 5)
- [`UnderstandStage`](understand_stage.md) / [`ReadStage`](read_stage.md) — write blackboard context consumed here
- [`InjectionStage`](injection_stage.md) — similar plan validation logic
- [`stages.md`](stages.md) — six-phase overview
