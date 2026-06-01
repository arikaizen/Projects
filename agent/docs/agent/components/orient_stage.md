# OrientStage

`src/agent/stages/orient_stage.hpp` · `src/agent/stages/orient_stage.cpp`

## Overview

`OrientStage` is **Step 2A** of the six-phase agent loop. It surveys the available tool catalog, completed history, and the structured understanding from the blackboard to produce a situational picture: which tools are relevant, what prior context already exists, and what the overall approach should be. The result is written to the blackboard under `"agent:orientation"` and the stage chains to `LocateStage`.

## Factory Registration

```
name: "OrientStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override for the agent's default task |

## Execution

1. Reads `task` from inputs or ctx config.
2. Reads `"agent:understanding"` from the blackboard (set by `UnderstandStage`).
3. Renders `orient_stage.md` with `{{TASK}}`, `{{UNDERSTANDING}}`, `{{CATALOG}}`, `{{HISTORY}}`.
4. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=1024})`.
5. Parses the JSON orientation object.
6. Writes orientation to blackboard: `ctx.blackboard()->write("agent:orientation", orientation)`.
7. Pushes `LocateStage` (id: `"auto_locate"`) to the back of the queue.

## LLM Response Format

```json
{
  "relevant_tools":   ["GlobAction", "GrepAction", "ReadAction"],
  "existing_context": "none",
  "approach":         "Find the relevant source files, read them, then plan targeted edits.",
  "priorities":       ["locate source files", "identify the class structure"]
}
```

## Blackboard key written

`"agent:orientation"` — consumed by `LocateStage` and `ReadStage`.

## Related Components

- [`UnderstandStage`](understand_stage.md) — previous stage (Step 1)
- [`LocateStage`](locate_stage.md) — next stage (Step 2B)
- [`stages.md`](stages.md) — six-phase overview
