# ReadStage

`src/agent/stages/read_stage.hpp` · `src/agent/stages/read_stage.cpp`

## Overview

`ReadStage` is **Step 2C** of the six-phase agent loop. It receives the results of locate actions via `$ref`-resolved inputs, calls the LLM to synthesise a structured context summary, and writes it to the blackboard under `"agent:read_context"`. It then chains to `CodeIntelStage` (when code intelligence is needed) or directly to `ReasonStage`.

## Factory Registration

```
name: "ReadStage"
kind: Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `task` | string | No | Override for the agent's default task |
| `locate_results` | object | No | `$ref`-resolved outputs of locate actions (set by `LocateStage`) |
| `code_intel` | boolean | No | Force a `CodeIntelStage` pass before reasoning |

## Execution

1. Reads `task` and the resolved `locate_results` from inputs.
2. Reads `"agent:understanding"` and `"agent:orientation"` from blackboard.
3. Renders `read_stage.md` with `{{TASK}}`, `{{UNDERSTANDING}}`, `{{ORIENTATION}}`, `{{LOCATE_RESULTS}}`.
4. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=2048})`.
5. Parses the synthesis JSON and writes it to `"agent:read_context"` in the blackboard.
6. If `inputs["code_intel"] == true` or LLM returns `needs_code_intel: true`, pushes `CodeIntelStage`.
   Otherwise pushes `ReasonStage` (id: `"auto_reason"`).

## LLM Response Format

```json
{
  "summary":          "Found 3 source files and 12 grep matches for AgentManager...",
  "key_findings":     ["include/agent/agent_manager.hpp defines AgentManager"],
  "gaps":             ["test coverage not yet examined"],
  "needs_code_intel": true
}
```

## Blackboard key written

`"agent:read_context"` — consumed by `CodeIntelStage`, `ReasonStage`, and `RespondStage`.

## Related Components

- [`LocateStage`](locate_stage.md) — previous stage (Step 2B), supplies `locate_results`
- [`CodeIntelStage`](code_intel_stage.md) — optional next stage (Step 2E)
- [`ReasonStage`](reason_stage.md) — default next stage (Step 3)
- [`stages.md`](stages.md) — six-phase overview
