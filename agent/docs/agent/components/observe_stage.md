# ObserveStage

`src/agent/stages/observe_stage.hpp` · `src/agent/stages/observe_stage.cpp`

## Overview

`ObserveStage` is **Step 5** of the six-phase agent loop. It is pushed automatically by `ReasonStage` at the end of every plan, with `$ref` dependencies on all plan item IDs. This means `BatchExecutor` guarantees it runs only after every action in the plan has completed. It inspects the results, decides whether the task is done, and either chains to `RespondStage` or pushes a new `ReasonStage` for another iteration.

## Factory Registration

```
name: "ObserveStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `plan_results` | array | No | `$ref`-resolved outputs of all plan items (injected by `ReasonStage`) |

## How It Is Pushed

`ReasonStage.validateAndPushPlan` appends an `ObserveStage` to the back of the queue after every plan:

```cpp
obs_inputs["plan_results"] = ["$action1_id", "$action2_id", ...];
ctx.push(ObserveStage("observe_<reason_id>", obs_inputs), Back);
```

The `$ref` entries create DAG edges so `ObserveStage` is the last item to execute in that batch.

## Execution

1. Reads `plan_results` (resolved array of plan item outputs) from inputs.
2. Reads recent history summary.
3. Renders `observe_stage.md` with `{{TASK}}`, `{{PLAN_RESULTS}}`, `{{HISTORY}}`, `{{OUTPUT_SCHEMA}}`.
4. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=1024})`.
5. Parses the observation JSON.
6. If `done == true` or `next_action == "respond"`: pushes `RespondStage`.
7. If `next_action == "iterate"`: pushes a new `ReasonStage` (id: `"auto_reason_<iteration>"`) with the refined task from `next_task`.

## LLM Response Format

```json
{
  "done":         false,
  "observations": ["file written successfully", "tests still failing"],
  "failures":     ["BashAction test_run: exit code 1"],
  "next_action":  "iterate",
  "next_task":    "Fix the failing tests in tests/agent/test_loop.cpp"
}
```

Or when complete:

```json
{
  "done":         true,
  "observations": ["all 3 files written", "tests pass"],
  "failures":     [],
  "next_action":  "respond"
}
```

## Related Components

- [`ReasonStage`](reason_stage.md) — pushes ObserveStage after every plan (Step 3)
- [`RespondStage`](respond_stage.md) — next stage when done (Step 6)
- [`BatchExecutor`](batch_executor.md) — DAG ensures ObserveStage runs after all plan items
- [`stages.md`](stages.md) — six-phase overview
