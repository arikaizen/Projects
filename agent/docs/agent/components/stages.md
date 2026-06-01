# Stages (overview)

`src/agent/stages/` — LLM-powered work items deriving from [`Stage`](stage.md) → [`WorkItem`](work_item.md).

Each stage renders a prompt template via [`PromptLoader`](prompt_loader.md), calls the LLM through [`LLMClient`](llm_client.md), interprets the response, and may push new work items onto the queue. All four are registered with the [`WorkFactory`](work_factory.md) during `AgentManager` construction.

> Each stage has its own detailed page — this is just the index.

| Stage | Factory name | Role | Pushes to | Doc |
|---|---|---|---|---|
| ReasonStage | `ReasonStage` | Plan from full agent state | back | [reason_stage.md](reason_stage.md) |
| InjectionStage | `InjectionStage` | React to one prior result | front | [injection_stage.md](injection_stage.md) |
| TransformStage | `TransformStage` | LLM text transform (no queue side-effects) | — | [transform_stage.md](transform_stage.md) |
| ValidateStage | `ValidateStage` | Validate a result; optional corrective injection | front | [validate_stage.md](validate_stage.md) |

## Common shape

All stages set `WorkResult.item_kind = "Stage"`, emit `stage_start` / `stage_done` / `stage_error` events, and call the LLM in JSON mode (except `TransformStage`, which returns free text). The planning stages (`ReasonStage`, `InjectionStage`) validate each plan item against the factory (registered name, unique id, satisfied `$ref` deps) before pushing.

## Prompt templates

| Template | Stage | Key placeholders |
|---|---|---|
| `reason_stage.md` | ReasonStage | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{OUTPUT_SCHEMA}}` |
| `injection_stage.md` | InjectionStage | + `{{PREVIOUS_RESULT}}` |
| `transform_stage.md` | TransformStage | `{{INSTRUCTION}}`, `{{INPUT_TEXT}}` |
| `validate_stage.md` | ValidateStage | `{{TARGET_OUTPUT}}`, `{{CRITERIA}}` (+ corrective variants) |

## Related

- [Stage (base class)](stage.md) · [Actions overview](actions.md)
- [WorkFactory](work_factory.md) · [PromptLoader](prompt_loader.md) · [AgentContext](agent_context.md)
