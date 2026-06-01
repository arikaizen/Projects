# Stages

`src/agent/stages/` — LLM-powered work items deriving from [`Stage`](stage.md) → [`WorkItem`](work_item.md).

Stages call `ctx.llm().complete(request)` to invoke the language model. Within a batch, stages that depend on each other via `$ref` execute in dependency order; independent stages may run concurrently.

## The Six-Phase Loop

The agent loop is structured around six named phases. Each phase is a registered `Stage` type. Phases chain to the next by pushing the next stage onto the queue during their own `execute()`.

| Phase | Stage | Step | Role |
|---|---|---|---|
| 1 | `UnderstandStage` | Understand the goal | Parse task into structured objective, constraints, output type, domain |
| 2A | `OrientStage` | Orient | Survey available tools, history, and blackboard; identify approach |
| 2B | `LocateStage` | Locate | Identify and execute search actions to find relevant resources |
| 2C | `ReadStage` | Read | Synthesise locate results into a context summary |
| 2D | `ValidateStage` | Verify *(optional)* | LLM-powered validation with optional corrective injection |
| 2E | `CodeIntelStage` | Code intelligence *(optional)* | Analyse code structure, types, call graph |
| 3 | `ReasonStage` | Reason & decide | LLM plans a batch of work items; auto-appends ObserveStage |
| 4 | *(BatchExecutor)* | Execute | Parallel action execution — no stage needed |
| 5 | `ObserveStage` | Observe | Inspect plan results; decide done or iterate |
| 6 | `RespondStage` | Respond | Compose final answer; set `should_stop = true` |

### Chain Diagram

```
UnderstandStage
    └─► OrientStage
            └─► LocateStage
                    ├─► [locate actions run in parallel]
                    └─► ReadStage  (depends on all locate actions via $ref)
                            ├─► CodeIntelStage (optional)  ─────┐
                            └────────────────────────────────────┤
                                                                 ▼
                                                          ReasonStage
                                                                 │
                                                         [plan actions run]
                                                                 │
                                                          ObserveStage  (depends on all plan actions via $ref)
                                                                 │
                                              ┌──────────────────┴──────────────────┐
                                              ▼                                     ▼
                                        RespondStage                     new ReasonStage (iterate)
                                     (sets should_stop)
```

### Fallback Stages

`InjectionStage` and `TransformStage` are utility stages that can be included in any `ReasonStage` plan when the LLM requires reactive injection or text transformation mid-plan.

## All Registered Stages

| Stage | Factory name | Prompt template | LLM mode | Doc |
|---|---|---|---|---|
| UnderstandStage | `UnderstandStage` | `understand_stage.md` | `json_mode=true` | [understand_stage.md](understand_stage.md) |
| OrientStage | `OrientStage` | `orient_stage.md` | `json_mode=true` | [orient_stage.md](orient_stage.md) |
| LocateStage | `LocateStage` | `locate_stage.md` | `json_mode=true` | [locate_stage.md](locate_stage.md) |
| ReadStage | `ReadStage` | `read_stage.md` | `json_mode=true` | [read_stage.md](read_stage.md) |
| ValidateStage | `ValidateStage` | `validate_stage.md` | `json_mode=true` | [validate_stage.md](validate_stage.md) |
| CodeIntelStage | `CodeIntelStage` | `code_intel_stage.md` | `json_mode=true` | [code_intel_stage.md](code_intel_stage.md) |
| ReasonStage | `ReasonStage` | `reason_stage.md` | `json_mode=true` | [reason_stage.md](reason_stage.md) |
| ObserveStage | `ObserveStage` | `observe_stage.md` | `json_mode=true` | [observe_stage.md](observe_stage.md) |
| RespondStage | `RespondStage` | `respond_stage.md` | `json_mode=true` | [respond_stage.md](respond_stage.md) |
| InjectionStage | `InjectionStage` | `injection_stage.md` | `json_mode=true` | [injection_stage.md](injection_stage.md) |
| TransformStage | `TransformStage` | `transform_stage.md` | `json_mode=false` | [transform_stage.md](transform_stage.md) |
| PlanCacheCheckStage | `PlanCacheCheckStage` | `plan_cache_check_stage.md` | `json_mode=true` | [plan_cache_check_stage.md](plan_cache_check_stage.md) |
| ReplayStage | `ReplayStage` | *(none — no LLM call)* | — | [replay_stage.md](replay_stage.md) |
| PlanAdaptStage | `PlanAdaptStage` | `plan_adapt_stage.md` | `json_mode=true` | [plan_adapt_stage.md](plan_adapt_stage.md) |

## Common Behavior

All stages share the same structural pattern:

1. Record `item_kind = "Stage"` and `timestamp` on the result.
2. Emit a `stage_start` event on the `EventBus`.
3. Render a system prompt via `ctx.promptLoader().render(template_name, vars)`.
4. Call `ctx.llm().complete({system_prompt, user_msg, json_mode, temperature, max_tokens})`.
5. Parse the response, write to blackboard or push items as appropriate.
6. Set `result.success`, `result.output`, `result.error`.
7. Record `duration` via a `steady_clock` pair.
8. Emit `stage_done` or `stage_error` on the `EventBus`.

## `final_answer` Termination

`ReasonStage` and `InjectionStage` can terminate the agent by setting `ctx.should_stop = true` and populating `ctx.final_output` when the LLM returns a top-level `{"final_answer": "..."}` or a plan item carries a `"final_answer"` field. `RespondStage` is the preferred termination path for the full six-phase loop.

## Prompt Templates

Templates live in `AgentManager::Config::prompts_dir`. Missing templates throw `std::runtime_error` at render time. Templates use `{{PLACEHOLDER}}` substitution.

| Stage | Required placeholders |
|---|---|
| `UnderstandStage` | `{{TASK}}` |
| `OrientStage` | `{{TASK}}`, `{{UNDERSTANDING}}`, `{{CATALOG}}`, `{{HISTORY}}` |
| `LocateStage` | `{{TASK}}`, `{{UNDERSTANDING}}`, `{{ORIENTATION}}`, `{{OUTPUT_SCHEMA}}` |
| `ReadStage` | `{{TASK}}`, `{{UNDERSTANDING}}`, `{{ORIENTATION}}`, `{{LOCATE_RESULTS}}` |
| `CodeIntelStage` | `{{TASK}}`, `{{READ_CONTEXT}}`, `{{HISTORY}}` |
| `ReasonStage` | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{OUTPUT_SCHEMA}}` |
| `ObserveStage` | `{{TASK}}`, `{{PLAN_RESULTS}}`, `{{HISTORY}}`, `{{OUTPUT_SCHEMA}}` |
| `RespondStage` | `{{TASK}}`, `{{HISTORY}}`, `{{READ_CONTEXT}}`, `{{CODE_INTEL}}` |
| `InjectionStage` | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{PREVIOUS_RESULT}}`, `{{OUTPUT_SCHEMA}}` |
| `TransformStage` | `{{INSTRUCTION}}`, `{{INPUT_TEXT}}` |
| `ValidateStage` | `{{TARGET_OUTPUT}}`, `{{CRITERIA}}` (+ `{{CATALOG}}`, `{{OUTPUT_SCHEMA}}` when `corrective_injection=true`) |
| `PlanCacheCheckStage` | `{{TASK}}`, `{{CACHED_TASK}}`, `{{CACHED_FINGERPRINT}}`, `{{OUTPUT_SCHEMA}}` |
| `PlanAdaptStage` | `{{TASK}}`, `{{CACHED_PLAN}}`, `{{CHANGED_ASPECTS}}`, `{{CATALOG}}`, `{{OUTPUT_SCHEMA}}` |

## Related Components

- [`Stage`](stage.md) — base class
- [`LLMClient`](llm_client.md) — called by all stages
- [`PromptLoader`](prompt_loader.md) — renders templates
- [`WorkFactory`](work_factory.md) — all stages register here
- [`AgentContext`](agent_context.md) — queue, history, blackboard, LLM access
- [`Blackboard`](blackboard.md) — shared context written by setup stages and read by Reason/Respond
