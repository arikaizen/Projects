# TaskAction

`src/agent/actions/task_action.hpp` · `src/agent/actions/task_action.cpp`
**Factory name:** `TaskAction` · **Kind:** Action

---

## Purpose

Spawns a **sub-agent** to execute a task and waits for its result — the core of **Pattern A** (delegation). Enforces `max_depth` from `AgentConfig` to prevent unbounded recursion.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `task` | string | **yes** | Task description for the sub-agent |
| `agent_name` | string | no | Display name for the sub-agent |

## Output

The sub-agent's final output JSON is returned directly as this action's output (`result.output = sub_result`).

## Depth Enforcement

The sub-agent is spawned with `current_depth + 1`. When `current_depth >= max_depth`, the action fails rather than recursing further.

## Thread-Safety

`AgentManager` methods are individually thread-safe, so concurrent `TaskAction`s on the pool are safe. Sub-agents run on their own dedicated threads (see [Concurrency](../concurrency.md)).

## Related

- [Actions overview](actions.md) · [AgentManager](agent_manager.md) — spawn/run APIs
- [Concurrency](../concurrency.md) — sub-agent threading and the inline fast-path
- [WorkItem](work_item.md)
