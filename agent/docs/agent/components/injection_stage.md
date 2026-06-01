# InjectionStage

`src/agent/stages/injection_stage.hpp` · `src/agent/stages/injection_stage.cpp`

## Overview

`InjectionStage` is a meta-reasoning step. Unlike `ReasonStage` which surveys the full agent state, `InjectionStage` focuses on a single previous result and asks the LLM "given this output, what should happen next?" Items are injected at the **front** of the queue (high-priority), not the back.

## Factory Registration

```
name:  "InjectionStage"
kind:  Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `target_id` | string | No | ID of result to inspect; defaults to the last result |
| `task` | string | No | Override task description for this step |

## Execution

1. Locates the target result: `inputs["target_id"]` if present, otherwise `ctx.lastResult()`.
2. Gathers template variables: `CATALOG`, `HISTORY`, `QUEUE`, `TASK`, `PREVIOUS_RESULT` (the target's output JSON), `OUTPUT_SCHEMA`.
3. Renders `injection_stage.md` via `PromptLoader`.
4. Calls `ctx.llm().complete({system_prompt, user_msg, json_mode=true, temperature=0.3, max_tokens=4096})`.
5. Parses the JSON response (same format as `ReasonStage`).
6. Validates each plan item.
7. Pushes items to the **front** of the queue in reverse order so execution order matches plan order.

## Front-of-Queue Push

Items are collected, validated, then pushed in reverse order to `Position::Front`:

```
Plan: [A, B, C]  →  push C to front → push B to front → push A to front
Queue: [A, B, C, ...existing...]
```

This ensures the injected items execute before anything already in the queue.

## Output

| Field | Value |
|---|---|
| `success` | `true` on successful LLM call and valid plan |
| `output.injected_count` | Number of items injected |
| `output.plan` | The raw plan array |

On termination path: `output` is `{"answer": "..."}` and `ctx.should_stop = true`.

## Events Emitted

| Event type | When |
|---|---|
| `stage_start` | Before the LLM call |
| `stage_done` | After successful execution |
| `stage_error` | On LLM failure, missing target, or invalid plan |
| `agent_final_answer` | When a `final_answer` is received |

## Validation

Same rules as `ReasonStage`:
1. Every `name` must be registered in `WorkFactory`.
2. Every `id` must be unique across history and the current plan.
3. Every `$ref` must point to a known id.

## Related Components

- [`Stage`](stage.md) / [`stages.md`](stages.md) — base class and overview
- [`ReasonStage`](reason_stage.md) — same validation logic, but pushes to the back
- [`AgentContext`](agent_context.md) — `push(..., Position::Front)` used here
- [`PromptLoader`](prompt_loader.md) — renders `injection_stage.md`
