# ReplayStage

`src/agent/stages/replay_stage.hpp` · `src/agent/stages/replay_stage.cpp`

## Overview

`ReplayStage` replays a cached successful plan without any LLM call. It remaps every item ID to a fresh `"replay_N"` prefix to avoid history-ID collisions, rewrites all `$ref` strings in inputs to use the new IDs, pushes the remapped items, writes the original steps back to `agent:last_plan`, and finally wires `ObserveStage` with `$ref` deps on all replayed items.

`ObserveStage` then verifies the replayed execution: if the results still satisfy the task, it increments `run_count` in the cache and chains to `RespondStage`; if not, it iterates with a new `ReasonStage`.

## Factory Registration

```
name: "ReplayStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `steps` | array | Yes | Cached plan steps from `PlanCache.steps` |
| `task`  | string | No | Current task string (metadata only) |

## ID Remapping

For a cached plan `[{id: "b1"}, {id: "r1", inputs: {path: "$b1.stdout"}}]`, `ReplayStage` produces:

```
old_id → new_id
b1     → replay_0
r1     → replay_1
```

And rewrites `"$b1.stdout"` → `"$replay_0.stdout"` throughout inputs, recursively through all nested objects and arrays.

## Execution

1. Reads `inputs["steps"]`.
2. Builds `id_map: old_id → "replay_N"`.
3. For each step: creates work item with new ID + remapped inputs, pushes to queue.
4. Writes original `steps` to `agent:last_plan` on the blackboard.
5. Pushes `ObserveStage` (id: `"observe_<replay_id>"`) with `$ref` deps on all replayed IDs.

## Output

| Field | Value |
|---|---|
| `success` | `true` when all steps were pushed |
| `output.replayed_count` | Number of items pushed |

## Related Components

- [`PlanCacheCheckStage`](plan_cache_check_stage.md) — pushes this stage on `"same"` match
- [`ObserveStage`](observe_stage.md) — auto-wired after all replayed items
- [`PlanCache`](plan_cache.md) — source of the `steps` data
