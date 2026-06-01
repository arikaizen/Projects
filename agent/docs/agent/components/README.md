# Component Reference

Detailed documentation for every component in the agent engine.

## Core Orchestration

| Component | File | Description |
|---|---|---|
| [AgentManager](agent_manager.md) | `include/agent/agent_manager.hpp` | Central orchestrator; owns all shared infrastructure |
| [Agent & AgentContext](agent.md) | `include/agent/agent.hpp` | Per-agent event loop and runtime state |
| [BatchExecutor](batch_executor.md) | `include/agent/batch_executor.hpp` | DAG-based parallel work scheduler |
| [ThreadPool](thread_pool.md) | `include/agent/thread_pool.hpp` | Fixed-size worker thread pool (L3 concurrency) |

## Work Items

| Component | File | Description |
|---|---|---|
| [WorkItem, WorkResult & WorkFactory](work_item.md) | `include/agent/work_item.hpp` | Abstract base, result struct, and self-registering factory |
| [Stages](stages.md) | `src/agent/stages/` | LLM-powered reasoning steps (Reason, Injection, Transform, Validate) |
| [Actions](actions.md) | `src/agent/actions/` | Deterministic operations (13 built-in types) |

## Infrastructure

| Component | File | Description |
|---|---|---|
| [PromptLoader](prompt_loader.md) | `include/agent/prompt_loader.hpp` | Template loading, substitution, and hot reload |
| [EventBus](event_bus.md) | `include/agent/event_bus.hpp` | Synchronous publish/subscribe event bus |
| [Blackboard](blackboard.md) | `include/agent/blackboard.hpp` | Thread-safe shared key-value store (Pattern C) |
| [MessageInbox](message_inbox.md) | `include/agent/message_inbox.hpp` | Per-agent MPSC message queue (Pattern B) |
| [QuotaManager](quota_manager.md) | `include/agent/quota.hpp` | Per-user resource limits and RAII guards |
| [LLMClient & MemoryBackend](llm_client.md) | `include/agent/llm_client.hpp` | Abstract LLM and memory backend interfaces |

## Model Integration

| Component | File | Description |
|---|---|---|
| [AIModel (abstract base)](ai_model.md) | `third_party/ai_model/aimodel.hpp` | Abstract model: generation + embeddings + validation + ranking |
| [AIModelVLLM](ai_model_vllm.md) | `third_party/ai_model/aimodel_vllm.hpp` | HTTP / OpenAI-compatible backend (opt-in) |
| [AIModelLlama](ai_model_llama.md) | `third_party/ai_model/aimodel_llama.hpp` | Local GGUF backend via llama.cpp (opt-in) |
| [AIModelLLMClient](ai_model_llm_client.md) | `include/agent/ai_model_llm_client.hpp` | Adapter: `AIModel` → `LLMClient` |
| [AIModelMemoryBackend](ai_model_memory_backend.md) | `include/agent/ai_model_memory_backend.hpp` | Adapter: `AIModel` → `MemoryBackend` |

## Public Interface

| Component | File | Description |
|---|---|---|
| [C ABI](c_api.md) | `include/agent_engine/c_api.h` | Stable C interface for `libagent_engine.so` |

---

## Concurrency Levels

```
L1: Per-user QuotaManager     — agent / LLM / tool limits per user
L2: Dedicated std::thread     — one per Agent loop (AgentManager::AgentEntry::runner)
L3: ThreadPool                — shared pool used only by BatchExecutor
L4: fan-out futures           — AgentManager::fanOut / am_fan_out
```

See [concurrency.md](../concurrency.md) for the full explanation.

## Multi-Agent Patterns

```
Pattern A — Delegation/Pipe   — pipe(), TaskAction, am_pipe, am_run_agent
Pattern B — Messaging         — MessageInbox, SendMessageAction, ReceiveMessagesAction
Pattern C — Blackboard        — Blackboard, Blackboard*Action, am_blackboard_*
```
