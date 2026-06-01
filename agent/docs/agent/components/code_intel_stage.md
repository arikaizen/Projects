# CodeIntelStage

`src/agent/stages/code_intel_stage.hpp` · `src/agent/stages/code_intel_stage.cpp`

## Overview

`CodeIntelStage` is the optional **Step 2E** of the six-phase agent loop. It analyses code structure from prior `ReadAction` and `GrepAction` results in history — identifying types, call relationships, design patterns, and entry points — and writes the findings to the blackboard under `"agent:code_intel"`. It then chains to `ReasonStage`.

It is pushed by `ReadStage` when `inputs["code_intel"]` is `true` or when the LLM synthesis returns `needs_code_intel: true`. It can also be included directly in a `ReasonStage` plan when targeted code analysis is needed mid-task.

## Factory Registration

```
name: "CodeIntelStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override for the agent's default task |

## Execution

1. Reads `task` from inputs or ctx config.
2. Reads `"agent:read_context"` from the blackboard.
3. Reads the last 15 history entries (file contents, grep results).
4. Renders `code_intel_stage.md` with `{{TASK}}`, `{{READ_CONTEXT}}`, `{{HISTORY}}`.
5. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=2048})`.
6. Writes result to `"agent:code_intel"` in the blackboard.
7. Pushes `ReasonStage` (id: `"auto_reason"`).

## LLM Response Format

```json
{
  "structures":     [{"name": "AgentManager", "role": "orchestrates agent lifecycles"}],
  "entry_points":   ["src/agent/agent_manager.cpp:255 — runAgent"],
  "dependencies":   ["ThreadPool", "WorkFactory", "LLMClient"],
  "patterns":       ["RAII thread ownership", "opaque handle via unique_ptr"],
  "call_flow":      "runAgent creates a promise, spawns a thread, agent loop pops queue items...",
  "change_targets": ["src/agent/agent_manager.cpp:273"]
}
```

## Blackboard key written

`"agent:code_intel"` — consumed by `ReasonStage` (via blackboard) and `RespondStage`.

## Related Components

- [`ReadStage`](read_stage.md) — previous stage (Step 2C), decides whether to invoke this
- [`ReasonStage`](reason_stage.md) — next stage (Step 3)
- [`stages.md`](stages.md) — six-phase overview
