# RespondStage

`src/agent/stages/respond_stage.hpp` · `src/agent/stages/respond_stage.cpp`

## Overview

`RespondStage` is **Step 6** of the six-phase agent loop — the terminal stage. It composes a final, user-facing answer from the full execution history and any blackboard summaries, sets `ctx.final_output`, and sets `ctx.should_stop = true` to terminate the agent loop.

It is pushed by `ObserveStage` when the task is judged complete.

## Factory Registration

```
name: "RespondStage"
kind: Stage
```

**Input schema:** no inputs required.

## Execution

1. Reads `task` from ctx config.
2. Reads the last 30 history entries and blackboard keys `"agent:read_context"` and `"agent:code_intel"`.
3. Renders `respond_stage.md` with `{{TASK}}`, `{{HISTORY}}`, `{{READ_CONTEXT}}`, `{{CODE_INTEL}}`.
4. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.3, max_tokens=4096})`.
5. Extracts `response["answer"]` (or uses the raw content on parse failure).
6. Sets `ctx.final_output = {"answer": "..."}` and `ctx.should_stop = true`.
7. Emits `agent_final_answer` event.

## LLM Response Format

```json
{ "answer": "The complete response to the user..." }
```

## Output

| Field | Value |
|---|---|
| `success` | `true` on a successful LLM call |
| `output` | `{"answer": "..."}` — same as `ctx.final_output` |

After `RespondStage` completes, the agent loop terminates with `TerminationReason::ShouldStop`.

## Events Emitted

| Event type | When |
|---|---|
| `stage_start` | Before the LLM call |
| `agent_final_answer` | After answer is composed |
| `stage_done` | After execution |

## Related Components

- [`ObserveStage`](observe_stage.md) — pushes RespondStage when done (Step 5)
- [`Agent`](agent.md) — loop terminates via `ctx.should_stop`
- [`stages.md`](stages.md) — six-phase overview
