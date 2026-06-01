# UnderstandStage

`src/agent/stages/understand_stage.hpp` · `src/agent/stages/understand_stage.cpp`

## Overview

`UnderstandStage` is **Step 1** of the six-phase agent loop. It reads the raw task string and calls the LLM to produce a structured breakdown — objective, constraints, output type, domain, key entities, and whether code intelligence is needed. The result is written to the blackboard under `"agent:understanding"` for all downstream stages, and the stage chains to `OrientStage`.

## Factory Registration

```
name: "UnderstandStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override for the agent's default task |

## Execution

1. Reads `task` from `inputs["task"]` or falls back to `ctx.config().task`.
2. Renders `understand_stage.md` with `{{TASK}}`.
3. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.1, max_tokens=1024})`.
4. Parses the JSON response into a structured understanding object.
5. Writes the understanding to the blackboard: `ctx.blackboard()->write("agent:understanding", understanding)`.
6. Pushes `OrientStage` (id: `"auto_orient"`) to the back of the queue.

## LLM Response Format

```json
{
  "objective":        "single-sentence goal",
  "constraints":      ["constraint A", "constraint B"],
  "output_type":      "code",
  "domain":           "code",
  "key_entities":     ["AgentManager", "WorkFactory"],
  "needs_code_intel": true
}
```

## Output

| Field | Value |
|---|---|
| `success` | `true` on a successful LLM call |
| `output` | The parsed understanding object |

## Blackboard key written

`"agent:understanding"` — consumed by `OrientStage`, `LocateStage`, `ReadStage`, and `ReasonStage`.

## Related Components

- [`OrientStage`](orient_stage.md) — next stage in the chain (Step 2A)
- [`stages.md`](stages.md) — full six-phase stage overview
- [`PromptLoader`](prompt_loader.md) — renders `understand_stage.md`
