# AIModelLLMClient

`include/agent/ai_model_llm_client.hpp` · `src/agent/ai_model_llm_client.cpp`

---

## Overview

`AIModelLLMClient` adapts any concrete [`AIModel`](ai_model.md) (`AIModelLlama`, `AIModelVLLM`, or a custom subclass) to the engine's [`LLMClient`](llm_client.md) interface. It is the bridge that lets the agent engine run on a real model instead of the `MockLLMClient` stub.

```
AIModel::Generate(prompt, temperature, max_tokens)
        ▲
        │  AIModelLLMClient::complete()
        │
agent::LLMClient::Request{ system_prompt, user_message, json_mode, temperature, max_tokens }
        └──────────────► called by every Stage via ctx.llm().complete(req)
```

---

## Why the stateless `AIModel::Generate` layer

The `AI/convo/` module offers two layers: `AIModel` (stateless generation) and `AIConvo` (stateful conversation with its own history). The adapter targets `AIModel` deliberately:

- The agent engine **already owns** the conversation in `AgentContext::history`.
- Each `Stage` renders a **complete** system prompt every call (task + history summary + tool catalog + todo list).
- Using `AIConvo` would maintain a *second*, conflicting history.

So `AIModel::Generate` — a pure `(prompt) -> text` function — is the correct integration point.

---

## Construction

```cpp
// Borrowing: caller owns the model; it must outlive the adapter.
explicit AIModelLLMClient(AIModel& model);

// Owning: the adapter takes ownership, so the model lives as long as the
// client does. Used when handing the client to an AgentManager that outlives
// the caller's scope (e.g. inside am_create).
explicit AIModelLLMClient(std::unique_ptr<AIModel> model);
```

| Constructor | Ownership | Typical use |
|---|---|---|
| `AIModelLLMClient(AIModel&)` | Borrowed reference | Tests; app keeps the model on the stack |
| `AIModelLLMClient(unique_ptr<AIModel>)` | Owned | `am_create`; model lifetime tied to client |

---

## `complete`

```cpp
Response complete(const Request& req) override;
```

Behaviour:

1. **Folds** `system_prompt` + `"\n\n"` + `user_message` into one prompt string (because `AIModel::Generate` takes a single prompt; a backend needing a chat template applies it inside its own `RawGenerate`).
2. If `req.json_mode` is set, appends an instruction asking for JSON-only output (`AIModel` has no native JSON-mode switch).
3. Calls `m_model.Generate(prompt, req.temperature, req.max_tokens)`.
4. Returns `Response{ content, success=true, error="" }` on success.
5. **Catches all exceptions** — `AIModel` throws on blank/empty/invalid input — and returns `Response{ "", success=false, error=<what> }`. No exception escapes the adapter, satisfying the engine's contract that a stage can always inspect `resp.success`.

### `modelName`

```cpp
std::string modelName() const override;  // returns m_model.GetModelName()
```

---

## Usage

### Direct (C++)

```cpp
#include "agent/ai_model_llm_client.hpp"
#include "ai_model/aimodel_vllm.hpp"   // requires AGENT_ENABLE_VLLM

AIModelVLLM model("http://localhost:8000", "meta-llama/Llama-3-8b-instruct");
auto llm = std::make_shared<agent::AIModelLLMClient>(model);   // borrowing
AgentManager manager(config, llm);
```

### Via the C ABI

`am_create` constructs this adapter automatically when the config selects a compiled-in backend:

```json
{
  "thread_pool_size": 16,
  "llm": {
    "backend":     "vllm",
    "base_url":    "http://localhost:8000",
    "model":       "meta-llama/Llama-3-8b-instruct",
    "api_key":     "",
    "embed_model": "",
    "max_context": 8192
  }
}
```

If `backend` is absent (or the engine was built without that backend), `am_create` falls back to the stub `MockLLMClient`. See [C ABI](c_api.md).

---

## Replaces

| Stub | Replaced by |
|---|---|
| `MockLLMClient` (tests, default `am_create`) | `AIModelLLMClient` wrapping a real `AIModel` |

---

## Related Components

- [`AIModel`](ai_model.md) — the model hierarchy being adapted
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — sibling adapter for memory
- [`LLMClient`](llm_client.md) — the interface implemented here
- [`Stages`](stages.md) — callers via `ctx.llm().complete(...)`
- [`AgentManager`](agent_manager.md) — receives the client at construction
- [C ABI](c_api.md) — `am_create` builds this when `AGENT_HAS_VLLM` is defined
