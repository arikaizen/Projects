# PlanCacheCheckStage

`src/agent/stages/plan_cache_check_stage.hpp` · `src/agent/stages/plan_cache_check_stage.cpp`

## Overview

`PlanCacheCheckStage` is the entry point for agent re-runs. `AgentManager::runAgent` pushes it first whenever a `PlanCache` file already exists for the agent. It loads the cached plan, compares the current task against the cached task, and routes to one of three paths:

| Route | Condition | Next stage |
|---|---|---|
| **Replay** | Task string is identical (no LLM) OR LLM returns `"same"` | `ReplayStage` |
| **Adapt** | LLM returns `"changed"` | `PlanAdaptStage` |
| **Fresh start** | No cache, LLM call fails, or LLM returns `"different"` | `UnderstandStage` |

## Factory Registration

```
name: "PlanCacheCheckStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override task (defaults to `ctx.config().task`) |

## Execution

1. Loads cache via `ctx.planCache()->load(agent_id)`. If `planCache()` is null or no entry exists, pushes `UnderstandStage` and returns.
2. **Exact match fast path**: if `current_task == cached.task` (string equality), pushes `ReplayStage` immediately — no LLM call.
3. **Differs**: renders `plan_cache_check_stage.md` with `{{TASK}}`, `{{CACHED_TASK}}`, `{{CACHED_FINGERPRINT}}`, calls LLM (`json_mode=true`, `temperature=0.1`, `max_tokens=512`).
4. Parses `{"match": "same"|"changed"|"different", "reason": "...", "changed_aspects": [...]}`.
5. Routes based on `match`.

## LLM Response Format

```json
{
  "match": "changed",
  "reason": "The target file path changed",
  "changed_aspects": ["file path changed from src/foo.cpp to src/bar.cpp"]
}
```

## Output

| Field | Value |
|---|---|
| `route` | `"replay"`, `"adapt"`, or `"fresh"` |
| `reason` | Human-readable justification |
| `run_count` | Replay count from cache (replay path only) |
| `changed_aspects` | Array of change descriptions (adapt path only) |

## Related Components

- [`PlanCache`](plan_cache.md) — loaded here
- [`ReplayStage`](replay_stage.md) — pushed on `"same"` match
- [`PlanAdaptStage`](plan_adapt_stage.md) — pushed on `"changed"` match
- [`UnderstandStage`](understand_stage.md) — pushed on `"different"` or no cache
- [`AgentManager`](agent_manager.md) — decides whether to push this stage or UnderstandStage
