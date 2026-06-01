# TransformStage

`src/agent/stages/transform_stage.hpp` · `src/agent/stages/transform_stage.cpp`
**Factory name:** `TransformStage` · **Kind:** Stage · **Prompt:** `prompts/transform_stage.md`

---

## Purpose

A general-purpose LLM text transformation. It applies a natural-language `instruction` to an input `text` and returns the transformed string. Unlike the planning stages, it produces **free text** (JSON mode is off) and does not push new work items.

Typical uses: summarise, translate, reformat, extract.

---

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `instruction` | string | **yes** | What to do to the text |
| `text` | string | **yes** | The text to transform — may be a `$ref` to a prior result's string field |

```json
{
  "name": "TransformStage", "id": "summary",
  "inputs": {"instruction": "Summarise in 3 bullets", "text": "$fetch.body"}
}
```

Both inputs are run through `resolveReferences` first, so `text` (and even `instruction`) can be `$ref` values. Missing or non-string inputs throw `std::invalid_argument`.

---

## Execution

1. `resolveReferences(inputs)` — resolves any `$ref` tokens against history.
2. Renders `transform_stage.md` with `{{INSTRUCTION}}` and `{{INPUT_TEXT}}`.
3. Calls the LLM with **`json_mode=false`** (`temperature=0.5`, `max_tokens=4096`).
4. Returns the raw completion as the transformed text.

---

## Result Output

```json
{"transformed_text": "• point one\n• point two\n• point three"}
```

---

## Parallelism

`TransformStage` has no side effects on the queue, so multiple independent `TransformStage` items in one batch run **concurrently** on the thread pool (subject to their `$ref` dependencies).

---

## Events Emitted

`stage_start`, `stage_done`, `stage_error`.

---

## Related

- [Stages overview](stages.md) · [ReasonStage](reason_stage.md) · [InjectionStage](injection_stage.md) · [ValidateStage](validate_stage.md)
- [AgentContext](agent_context.md) — `$ref` resolution
- [BatchExecutor](batch_executor.md) — parallel execution
- [PromptLoader](prompt_loader.md) · [LLMClient](llm_client.md)
