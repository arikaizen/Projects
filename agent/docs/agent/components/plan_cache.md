# PlanCache

`include/agent/plan_cache.hpp` · `src/agent/plan_cache.cpp`

## Overview

`PlanCache` provides file-backed persistent storage for the plan a single-purpose agent used to successfully complete its task. On subsequent runs the agent can replay the stored plan directly (no LLM call) or ask the LLM to adapt only the steps affected by changed parameters.

One JSON file is written per `agent_id` under `AgentManager::Config::cache_dir` (default `./agent_cache`).

## Data Structure

```cpp
struct CachedPlan {
    std::string    task;         // original task string
    nlohmann::json fingerprint;  // agent:understanding from the successful run
    nlohmann::json steps;        // raw plan array (pre-$ref-resolution)
    std::string    created_at;   // ISO-8601 UTC
    int            run_count;    // incremented on each successful replay
};
```

| Field | Written by | Used by |
|---|---|---|
| `task` | `ObserveStage` (on done) | `PlanCacheCheckStage` for exact-match check |
| `fingerprint` | `ObserveStage` (on done) | `PlanCacheCheckStage` for LLM-assisted comparison |
| `steps` | `ObserveStage` (on done) | `ReplayStage`, `PlanAdaptStage` |
| `run_count` | `ObserveStage` (increments) | Telemetry only |

## API

```cpp
// Persists a plan for agent_id (overwrites any prior cache).
void save(const std::string& agent_id, const CachedPlan& plan);

// Loads the cached plan for agent_id. Returns nullopt if none exists.
std::optional<CachedPlan> load(const std::string& agent_id) const;

// Deletes the cache file for agent_id.
void clear(const std::string& agent_id);

// Returns true if a cache file exists for agent_id.
bool exists(const std::string& agent_id) const;
```

All methods are thread-safe.

## Configuration

`AgentManager::Config::cache_dir` controls the storage directory (created automatically):

```cpp
AgentManager::Config cfg;
cfg.cache_dir = "./my_cache";   // default: "./agent_cache"
```

## Related Components

- [`PlanCacheCheckStage`](plan_cache_check_stage.md) — reads cache on each run to decide replay/adapt/fresh
- [`ReplayStage`](replay_stage.md) — replays `steps` directly when task unchanged
- [`PlanAdaptStage`](plan_adapt_stage.md) — adapts `steps` when parameters changed
- [`ObserveStage`](observe_stage.md) — writes to cache when `done=true`
- [`ReasonStage`](reason_stage.md) — writes `agent:last_plan` to blackboard for ObserveStage to pick up
