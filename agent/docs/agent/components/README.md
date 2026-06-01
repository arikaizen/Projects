# Component Reference

Detailed documentation — **one file per component** in the agent engine.

## Core Orchestration

| Component | Source | Doc |
|---|---|---|
| AgentManager | `include/agent/agent_manager.hpp` | [agent_manager.md](agent_manager.md) |
| Agent | `include/agent/agent.hpp` | [agent.md](agent.md) |
| AgentContext | `include/agent/agent_context.hpp` | [agent_context.md](agent_context.md) |
| BatchExecutor | `include/agent/batch_executor.hpp` | [batch_executor.md](batch_executor.md) |
| ThreadPool | `include/agent/thread_pool.hpp` | [thread_pool.md](thread_pool.md) |

## Work Items

| Component | Source | Doc |
|---|---|---|
| WorkItem & WorkResult | `include/agent/work_item.hpp` | [work_item.md](work_item.md) |
| WorkFactory | `include/agent/work_factory.hpp` | [work_factory.md](work_factory.md) |
| Stage (base) | `include/agent/stage.hpp` | [stage.md](stage.md) |
| Action (base) | `include/agent/action.hpp` | [action.md](action.md) |

### Stages (`src/agent/stages/`)

| Component | Doc |
|---|---|
| ReasonStage | [reason_stage.md](reason_stage.md) |
| InjectionStage | [injection_stage.md](injection_stage.md) |
| TransformStage | [transform_stage.md](transform_stage.md) |
| ValidateStage | [validate_stage.md](validate_stage.md) |
| _overview_ | [stages.md](stages.md) |

### Actions (`src/agent/actions/`)

| Component | Doc |
|---|---|
| BashAction | [bash_action.md](bash_action.md) |
| ReadAction | [read_action.md](read_action.md) |
| WriteAction | [write_action.md](write_action.md) |
| EditAction | [edit_action.md](edit_action.md) |
| GlobAction | [glob_action.md](glob_action.md) |
| GrepAction | [grep_action.md](grep_action.md) |
| WebFetchAction | [web_fetch_action.md](web_fetch_action.md) |
| WebSearchAction | [web_search_action.md](web_search_action.md) |
| TaskAction | [task_action.md](task_action.md) |
| TodoWriteAction | [todo_write_action.md](todo_write_action.md) |
| Messaging actions | [messaging_actions.md](messaging_actions.md) |
| Blackboard actions | [blackboard_actions.md](blackboard_actions.md) |
| Memory actions | [memory_actions.md](memory_actions.md) |
| MCPToolAction | [mcp_tool_action.md](mcp_tool_action.md) |
| _overview_ | [actions.md](actions.md) |

## Infrastructure

| Component | Source | Doc |
|---|---|---|
| PromptLoader | `include/agent/prompt_loader.hpp` | [prompt_loader.md](prompt_loader.md) |
| EventBus | `include/agent/event_bus.hpp` | [event_bus.md](event_bus.md) |
| Blackboard | `include/agent/blackboard.hpp` | [blackboard.md](blackboard.md) |
| MessageInbox | `include/agent/message_inbox.hpp` | [message_inbox.md](message_inbox.md) |
| QuotaManager | `include/agent/quota.hpp` | [quota_manager.md](quota_manager.md) |
| LLMClient | `include/agent/llm_client.hpp` | [llm_client.md](llm_client.md) |
| MemoryBackend | `include/agent/memory_backend.hpp` | [memory_backend.md](memory_backend.md) |

## Model Integration

| Component | Source | Doc |
|---|---|---|
| AIModel (abstract base) | `third_party/ai_model/aimodel.hpp` | [ai_model.md](ai_model.md) |
| AIModelVLLM | `third_party/ai_model/aimodel_vllm.hpp` | [ai_model_vllm.md](ai_model_vllm.md) |
| AIModelLlama | `third_party/ai_model/aimodel_llama.hpp` | [ai_model_llama.md](ai_model_llama.md) |
| AIModelLLMClient | `include/agent/ai_model_llm_client.hpp` | [ai_model_llm_client.md](ai_model_llm_client.md) |
| AIModelMemoryBackend | `include/agent/ai_model_memory_backend.hpp` | [ai_model_memory_backend.md](ai_model_memory_backend.md) |

## Public Interface

| Component | Source | Doc |
|---|---|---|
| C ABI | `include/agent_engine/c_api.h` | [c_api.md](c_api.md) |

---

## Concurrency Levels

```
L1: Per-user QuotaManager     — agent / LLM / tool limits per user
L2: Dedicated std::thread     — one per Agent loop
L3: ThreadPool                — shared pool used only by BatchExecutor
L4: fan-out futures           — AgentManager::fanOut / am_fan_out
```

See [concurrency.md](../concurrency.md).

## Multi-Agent Patterns

```
Pattern A — Delegation/Pipe   — pipe(), TaskAction, am_pipe, am_run_agent
Pattern B — Messaging         — MessageInbox, SendMessageAction, ReceiveMessagesAction
Pattern C — Blackboard        — Blackboard, Blackboard*Action, am_blackboard_*
```
