# AIModelLLMClient

`include/agent/ai_model_llm_client.hpp` · `src/agent/ai_model_llm_client.cpp`

## Overview

`AIModelLLMClient` is an adapter that exposes any `AIModel` subclass to the agent engine through the `LLMClient` interface. It bridges `AIModel::Generate` (stateless, single-turn) to `LLMClient::complete` (system_prompt + user_message + options).

## Design Notes

- **Stateless generation**: The agent engine owns conversation state in `AgentContext::history`. Each stage builds a fully-rendered system prompt every call, so the correct layer to call is `AIModel::Generate` (stateless), not `AIConvo::Chat` (which tracks its own turn history and would duplicate the engine's state management).
- **Prompt folding**: `LLMClient::Request` has separate `system_prompt` and `user_message` fields; `AIModel::Generate` takes a single string. The adapter folds them: `system_prompt + "\n\n" + user_message`.
- **json_mode**: `AIModel` has no native JSON-mode switch. When `json_mode=true`, the adapter appends `"\n\nRespond with valid JSON only."` to the combined prompt.
- **Exception safety**: `AIModel::Generate` throws on blank or invalid input. The adapter catches all exceptions and converts them to `Response{success=false, error=...}`. No exceptions escape `complete()`.

## Constructors

### Borrowing (non-owning)

```cpp
explicit AIModelLLMClient(AIModel& model);
```

The caller owns the model and must ensure it outlives the adapter.

### Owning

```cpp
explicit AIModelLLMClient(std::unique_ptr<AIModel> model);
```

The adapter takes ownership of the model. Use this when handing the client to `AgentManager`, which may outlive the calling scope.

## `complete`

```cpp
Response complete(const Request& req) override;
```

1. Constructs `prompt = req.system_prompt + "\n\n" + req.user_message`.
2. If `req.json_mode`, appends `"\n\nRespond with valid JSON only."`.
3. Calls `m_model.Generate(prompt, req.temperature, req.max_tokens)`.
4. Returns `Response{content, true, ""}` on success.
5. On any exception: returns `Response{"", false, ex.what()}`.

## `modelName`

```cpp
std::string modelName() const override { return m_model.GetModelName(); }
```

Delegates to `AIModel::GetModelName()`.

## Example

```cpp
// Owning — model lifetime managed by the client
auto model = std::make_unique<AIModelVLLM>("http://localhost:8000", "llama-3.1-8b");
auto llm   = std::make_shared<AIModelLLMClient>(std::move(model));
auto mgr   = AgentManager(config, llm);
```

```cpp
// Borrowing — model owned externally
AIModelLlama model("/models/llama.gguf");
AIModelLLMClient client(model);
// model must outlive client
```

## Related Components

- [`LLMClient`](llm_client.md) — the interface this class implements
- [`AIModel`](ai_model.md) — the model hierarchy this class wraps
- [`AIModelVLLM`](ai_model_vllm.md), [`AIModelLlama`](ai_model_llama.md) — concrete models
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — companion adapter for memory
- [`AgentManager`](agent_manager.md) — receives the `LLMClient` at construction
