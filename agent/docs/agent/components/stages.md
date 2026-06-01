# Stages (overview)

`src/agent/stages/` — LLM-powered work items deriving from [`Stage`](stage.md) → [`WorkItem`](work_item.md).

Stages call `ctx.llm().complete(request)` to invoke the language model. They execute **sequentially** within the agent loop: each stage sees the full prior history before the next one starts.

| Stage | Factory name | Prompt template | LLM mode | Doc |
|---|---|---|---|---|
| ReasonStage | `ReasonStage` | `reason_stage.md` | `json_mode=true` | [reason_stage.md](reason_stage.md) |
| InjectionStage | `InjectionStage` | `injection_stage.md` | `json_mode=true` | [injection_stage.md](injection_stage.md) |
| TransformStage | `TransformStage` | `transform_stage.md` | `json_mode=false` | [transform_stage.md](transform_stage.md) |
| ValidateStage | `ValidateStage` | `validate_stage.md` | `json_mode=true` | [validate_stage.md](validate_stage.md) |

## Common Behavior

All four stages share the same structural pattern:

1. Record `item_kind = "Stage"` and `timestamp` on the result.
2. Emit a `stage_start` event on the `EventBus`.
3. Render a system prompt via `ctx.promptLoader().render(template_name, vars)`.
4. Call `ctx.llm().complete({system_prompt, user_msg, json_mode, temperature, max_tokens})`.
5. Parse the response, push new items to the queue if appropriate.
6. Set `result.success`, `result.output`, `result.error`.
7. Record `duration` via a `steady_clock` pair.
8. Emit `stage_done` or `stage_error` on the `EventBus`.

## `final_answer` Termination

Any stage can terminate the agent by setting `ctx.should_stop = true` and populating `ctx.final_output`. This is triggered when:
- The LLM returns a top-level `{"final_answer": "..."}` object instead of a plan array, or
- A plan item carries a `"final_answer"` field.

The `EventBus` emits an `agent_final_answer` event in this case.

## Prompt Templates

Each stage requires one or more Markdown template files in `AgentManager::Config::prompts_dir`. Missing templates cause a `std::runtime_error` at render time. Templates use `{{PLACEHOLDER}}` substitution.

| Stage | Required placeholders |
|---|---|
| `ReasonStage` | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{OUTPUT_SCHEMA}}` |
| `InjectionStage` | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{PREVIOUS_RESULT}}`, `{{OUTPUT_SCHEMA}}` |
| `TransformStage` | `{{INSTRUCTION}}`, `{{INPUT_TEXT}}` |
| `ValidateStage` | `{{TARGET_OUTPUT}}`, `{{CRITERIA}}` (+ `{{CATALOG}}`, `{{OUTPUT_SCHEMA}}` when `corrective_injection=true`) |

## Related Components

- [`Stage`](stage.md) — base class
- [`LLMClient`](llm_client.md) — called by all four stages
- [`PromptLoader`](prompt_loader.md) — renders templates for all stages
- [`WorkFactory`](work_factory.md) — all four stages register themselves here
- [`AgentContext`](agent_context.md) — provides queue, history, and LLM access
