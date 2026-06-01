# InjectionStage

`src/agent/stages/injection_stage.hpp` · `src/agent/stages/injection_stage.cpp`
**Factory name:** `InjectionStage` · **Kind:** Stage · **Prompt:** `prompts/injection_stage.md`

---

## Purpose

A meta-stage that inspects **one previous result** and decides what to inject next. Where [`ReasonStage`](reason_stage.md) plans from the whole agent state, `InjectionStage` is narrower — "given this output, what follow-up work is needed?" — and pushes the resulting items to the **front** of the queue so they run before anything already queued.

---

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `target_id` | string | no | Id of the result to inspect; defaults to the **last** result |
| `task` | string | no | Overrides `AgentConfig::task` for this stage |

```json
{"name": "InjectionStage", "id": "inj1", "inputs": {"target_id": "fetch_step"}}
```

If `target_id` is given but no matching result exists, the stage fails. If omitted and there is no previous result, it fails.

---

## Execution

1. Resolves the target result (`resultById(target_id)` or `lastResult()`).
2. Renders `injection_stage.md` with `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{PREVIOUS_RESULT}}` (the target's output), `{{OUTPUT_SCHEMA}}`.
3. Calls the LLM in JSON mode (`temperature=0.3`, `max_tokens=4096`).
4. Interprets the response the same way as `ReasonStage` (top-level `final_answer` object, or an array plan).

### Front-injection ordering

The plan is validated left-to-right (same rules as `ReasonStage`), then pushed to the **front in reverse order**, so the final execution order matches the plan's declared order. A `final_answer` on the last entry stops the agent after the injected items run.

---

## Result Output

```json
{"injected_count": 2, "plan": [ ...the injected items... ]}
```
or `{"answer": "..."}` on a final answer.

---

## Events Emitted

`stage_start`, `stage_done`, `stage_error`, `agent_final_answer`.

---

## Related

- [Stages overview](stages.md) · [ReasonStage](reason_stage.md) · [TransformStage](transform_stage.md) · [ValidateStage](validate_stage.md)
- [AgentContext](agent_context.md) — `lastResult` / `resultById`, front-push
- [WorkFactory](work_factory.md) · [PromptLoader](prompt_loader.md) · [LLMClient](llm_client.md)
