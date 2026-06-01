# ValidateStage

`src/agent/stages/validate_stage.hpp` · `src/agent/stages/validate_stage.cpp`
**Factory name:** `ValidateStage` · **Kind:** Stage · **Prompt:** `prompts/validate_stage.md` (+ corrective variants)

---

## Purpose

LLM-powered validation of a previous result against stated `criteria`. Optionally, when validation fails, it asks the LLM for a **corrective plan** and injects those items at the front of the queue to self-heal.

---

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `target_id` | string | no | Id of the result to validate; defaults to the **last** result |
| `criteria` | string | **yes** | Natural-language validation criteria |
| `corrective_injection` | boolean | no (default `false`) | If `true` and invalid, inject corrective work items |

```json
{
  "name": "ValidateStage", "id": "check1",
  "inputs": {"target_id": "write1", "criteria": "Must be valid JSON with a 'name' field",
             "corrective_injection": true}
}
```

---

## Execution

1. Locate the target (`resultById(target_id)` or `lastResult()`); throws if absent.
2. Render the validation prompt. The loader tries `validate_stage` (or `validate_stage_corrective` when corrective injection is enabled) and **falls back** to the base `validate_stage` template if the variant file is missing.
   - Placeholders: `{{TARGET_OUTPUT}}`, `{{CRITERIA}}`, plus `{{CATALOG}}` / `{{OUTPUT_SCHEMA}}` when corrective.
3. Call the LLM in JSON mode (`temperature=0.2`, `max_tokens=2048`) expecting `{"valid": bool, "reason": "..."}`.
4. If invalid **and** `corrective_injection` is true:
   - Render a corrective-plan prompt (`validate_stage_corrective_plan`, with a fallback) including the failure `reason`.
   - Ask the LLM for a JSON array corrective plan.
   - Validate each item (registered name, unique id, satisfied `$ref` deps) and push to the **front** in reverse order.

---

## Result Output

```json
{"valid": false, "reason": "Missing 'name' field"}
```

When corrective items are injected, a `corrective_injection` event is emitted (with the count and reason); the result output still reports `valid`/`reason`.

---

## Prompt Templates Used

| Template | When |
|---|---|
| `validate_stage` | default validation; fallback for all variants |
| `validate_stage_corrective` | validation when `corrective_injection=true` (optional) |
| `validate_stage_corrective_plan` | generating the corrective plan (optional) |

Missing optional templates degrade gracefully to `validate_stage`.

---

## Events Emitted

`stage_start`, `stage_done`, `stage_error`, `validation_result` (target_id, valid, reason), `corrective_injection`.

---

## Related

- [Stages overview](stages.md) · [ReasonStage](reason_stage.md) · [InjectionStage](injection_stage.md) · [TransformStage](transform_stage.md)
- [AgentContext](agent_context.md) — target lookup, front-push
- [WorkFactory](work_factory.md) · [PromptLoader](prompt_loader.md) · [LLMClient](llm_client.md)
