# PlanAdaptStage

`src/agent/stages/plan_adapt_stage.hpp` · `src/agent/stages/plan_adapt_stage.cpp`

## Overview

`PlanAdaptStage` is called when `PlanCacheCheckStage` determines that the task has partially changed. It calls the LLM with the cached plan and a description of what changed, asks it to return a minimally modified plan, then validates and pushes that plan — exactly as `ReasonStage` does, but with the benefit of knowing which steps likely need to change and which can be reused.

## Factory Registration

```
name: "PlanAdaptStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `cached_plan` | array | Yes | `CachedPlan.steps` from `PlanCacheCheckStage` |
| `changed_aspects` | array | Yes | Change descriptions from `PlanCacheCheckStage` |
| `task` | string | No | Current task (defaults to `ctx.config().task`) |

## Execution

1. Reads `cached_plan`, `changed_aspects`, `task` from inputs.
2. Renders `plan_adapt_stage.md` with `{{TASK}}`, `{{CACHED_PLAN}}`, `{{CHANGED_ASPECTS}}`, `{{CATALOG}}`, `{{OUTPUT_SCHEMA}}`.
3. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.3, max_tokens=4096})`.
4. Parses the returned plan array.
5. Validates items (registered name, unique ID, valid `$ref` deps) and pushes them.
6. Writes adapted plan to `agent:last_plan` on the blackboard.
7. Pushes `ObserveStage` with `$ref` deps on all pushed items.

## LLM Prompt Guidance

The prompt instructs the LLM to:
- Reuse unchanged steps verbatim
- Update only the steps whose inputs reference changed parameters
- Add/remove steps only when parameter changes require it
- Preserve `$ref` dependency ordering

## Output

| Field | Value |
|---|---|
| `success` | `true` on successful LLM call and valid adapted plan |
| `output.adapted_size` | Number of items in the adapted plan |
| `output.plan` | The adapted plan array |

## Related Components

- [`PlanCacheCheckStage`](plan_cache_check_stage.md) — pushes this stage on `"changed"` match
- [`ReasonStage`](reason_stage.md) — identical validation/push pattern
- [`ObserveStage`](observe_stage.md) — auto-wired after all adapted items; saves to cache on success
- [`PlanCache`](plan_cache.md) — updated by ObserveStage on success
