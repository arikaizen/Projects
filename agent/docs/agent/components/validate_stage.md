# ValidateStage

`src/agent/stages/validate_stage.hpp` · `src/agent/stages/validate_stage.cpp`

## Overview

`ValidateStage` asks the LLM to evaluate a previous result against a set of criteria. It returns a `{"valid": bool, "reason": "..."}` response. Optionally, when validation fails, it makes a second LLM call to produce a corrective plan and injects those items at the front of the queue.

## Factory Registration

```
name:  "ValidateStage"
kind:  Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `target_id` | string | No | ID of result to validate; defaults to the last result |
| `criteria` | string | Yes | Validation criteria description for the LLM |
| `corrective_injection` | boolean | No | If `true` and validation fails, inject corrective work items |

## Execution

1. Locates the target result via `inputs["target_id"]` or `ctx.lastResult()`.
2. Extracts `criteria` from inputs.
3. Selects the prompt template:
   - `validate_stage_corrective` when `corrective_injection=true` (falls back to `validate_stage` if not found on disk).
   - `validate_stage` otherwise.
4. Calls `ctx.llm().complete({..., json_mode=true, temperature=0.2, max_tokens=2048})`.
5. Parses the response as `{"valid": bool, "reason": "..."}`.
6. If `!valid && corrective_injection`:
   - Makes a second LLM call requesting a corrective plan array.
   - Validates the plan using the same rules as `ReasonStage`.
   - Pushes valid corrective items to the **front** of the queue in reverse order.
7. Returns `output = {"valid": bool, "reason": "..."}` regardless of the corrective path.

## Output

| Field | Value |
|---|---|
| `success` | `true` as long as the LLM calls and JSON parsing succeeded |
| `output.valid` | `true` if the target result passes the criteria |
| `output.reason` | The LLM's explanation |

Note: `success=true` even when `valid=false`. `success=false` only when the LLM call itself fails or returns unparseable JSON.

## Events Emitted

| Event type | When |
|---|---|
| `stage_start` | Before the first LLM call |
| `stage_done` | After the stage completes |
| `stage_error` | On LLM failure or bad response format |
| `validation_result` | After parsing the validation response (includes `valid`, `reason`, `target_id`) |
| `corrective_injection` | After successfully injecting corrective items |

## Example

```json
{
  "name": "ValidateStage",
  "id": "v1",
  "inputs": {
    "target_id": "bash1",
    "criteria": "The command output must contain exactly one line starting with 'SUCCESS'",
    "corrective_injection": true
  }
}
```

## Related Components

- [`Stage`](stage.md) / [`stages.md`](stages.md) — base class and overview
- [`InjectionStage`](injection_stage.md) — similar corrective injection logic
- [`PromptLoader`](prompt_loader.md) — renders `validate_stage.md` (and optional `validate_stage_corrective`)
- [`LLMClient`](llm_client.md) — two LLM calls when `corrective_injection=true`
