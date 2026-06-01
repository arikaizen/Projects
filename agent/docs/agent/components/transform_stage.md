# TransformStage

`src/agent/stages/transform_stage.hpp` · `src/agent/stages/transform_stage.cpp`

## Overview

`TransformStage` applies a natural-language instruction to an input text using the LLM. Unlike `ReasonStage` and `InjectionStage`, it requests free-text output (`json_mode=false`) and returns the transformed text directly.

Typical use cases: reformat content, summarise, translate, extract information, or post-process action output.

## Factory Registration

```
name:  "TransformStage"
kind:  Stage
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `instruction` | string | Yes | Transformation instruction for the LLM (e.g. "summarise in 3 bullet points") |
| `text` | string | Yes | Text to transform. May be a `$ref` resolving to a string field from a previous result. |

## Execution

1. Calls `ctx.resolveReferences(inputs)` to expand any `$ref` values in `instruction` and `text`.
2. Validates that both `instruction` and `text` are present and are strings.
3. Renders `transform_stage.md` via `PromptLoader` with `{{INSTRUCTION}}` and `{{INPUT_TEXT}}`.
4. Calls `ctx.llm().complete({system_prompt, user_msg, json_mode=false, temperature=0.5, max_tokens=4096})`.
5. Returns the LLM's response as `output.transformed_text`.

## Output

| Field | Value |
|---|---|
| `success` | `true` on successful LLM call |
| `output.transformed_text` | The LLM's free-text output |

## Example

```json
{
  "name": "TransformStage",
  "id": "t1",
  "inputs": {
    "instruction": "Summarise in three bullet points",
    "text": "$read1.content"
  }
}
```

`$read1.content` resolves to the `content` field of the `ReadAction` result with id `read1`.

## Events Emitted

| Event type | When |
|---|---|
| `stage_start` | Before the LLM call |
| `stage_done` | After successful execution |
| `stage_error` | On LLM failure or missing/invalid inputs |

## Related Components

- [`Stage`](stage.md) / [`stages.md`](stages.md) — base class and overview
- [`PromptLoader`](prompt_loader.md) — renders `transform_stage.md`
- [`LLMClient`](llm_client.md) — the LLM call (free-text mode)
- [`AgentContext`](agent_context.md) — `resolveReferences` expands `$ref` in inputs
