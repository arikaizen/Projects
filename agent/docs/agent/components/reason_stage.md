# ReasonStage

`src/agent/stages/reason_stage.hpp` · `src/agent/stages/reason_stage.cpp`
**Factory name:** `ReasonStage` · **Kind:** Stage · **Prompt:** `prompts/reason_stage.md`

---

## Purpose

The primary reasoning step. Given the agent's full state — task, history, queue, and the tool catalog — it asks the LLM to produce an **ordered plan** of work items, then validates and pushes them onto the queue (at the **back**). This is the stage that drives the agent forward each iteration.

---

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `task` | string | no | Overrides `AgentConfig::task` for this stage only |

```json
{"name": "ReasonStage", "id": "reason1", "inputs": {}}
```

---

## Execution

1. Renders `reason_stage.md` with these placeholders:
   - `{{CATALOG}}` — `factory().toCatalogJson()` (all registered item types + schemas)
   - `{{HISTORY}}` — `historySummaryJson(20)`
   - `{{QUEUE}}` — `queueSummaryJson()`
   - `{{TASK}}` — the task string
   - `{{OUTPUT_SCHEMA}}` — the plan JSON schema (see below)
2. Calls the LLM in JSON mode (`temperature=0.3`, `max_tokens=4096`).
3. Interprets the response:
   - A top-level object with `final_answer` → sets `should_stop`, `final_output = {"answer": ...}`.
   - A JSON **array** → validated as a plan and pushed.
   - Anything else → failure.

### Plan validation (`validateAndPushPlan`)

Each plan item must:
- be a JSON object with string `name` and `id`;
- have a `name` that is **registered** in the `WorkFactory`;
- have an `id` unique across history and the rest of the plan;
- have every `$ref` dependency satisfied by an earlier history item or earlier plan item.

A plan item may carry a `final_answer` string; when present, that item is pushed and the agent stops after it.

---

## Output Schema (requested from the LLM)

```json
{
  "type": "array",
  "items": {
    "type": "object",
    "required": ["name", "id", "inputs"],
    "properties": {
      "name": {"type": "string"},
      "id": {"type": "string"},
      "inputs": {"type": "object"},
      "final_answer": {"type": "string"}
    }
  }
}
```

## Result Output

```json
{"plan_size": 3, "plan": [ ...the validated plan... ]}
```
or, when a top-level final answer is returned:
```json
{"answer": "..."}
```

---

## Events Emitted

`stage_start`, `stage_done` (with `success`), `stage_error`, `agent_final_answer`.

---

## Related

- [Stages overview](stages.md) · [InjectionStage](injection_stage.md) · [TransformStage](transform_stage.md) · [ValidateStage](validate_stage.md)
- [WorkFactory](work_factory.md) — validates and constructs plan items
- [AgentContext](agent_context.md) — history, queue, `$ref` resolution
- [PromptLoader](prompt_loader.md) · [LLMClient](llm_client.md)
