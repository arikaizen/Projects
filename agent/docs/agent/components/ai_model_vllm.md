# AIModelVLLM

`third_party/ai_model/aimodel_vllm.hpp` · `third_party/ai_model/aimodel_vllm.cpp`

> A concrete [`AIModel`](ai_model.md) backend. **Opt-in** — built only with `-DAGENT_ENABLE_VLLM=ON`.

---

## Overview

`AIModelVLLM` is an HTTP backend that talks to any **vLLM** server or other **OpenAI-compatible** API. It implements the [`AIModel`](ai_model.md) contract (`RawGenerate`, `RawEmbed`, `GetModelName`, `GetMaxContextLength`) by issuing JSON requests over `cpp-httplib` and parsing the responses with nlohmann/json.

This is the backend most useful for the agent engine in containerised / cloud settings: no local model weights, no GPU in-process — just a URL.

---

## Dependencies

| Dependency | Why |
|---|---|
| `httplib.h` (cpp-httplib, header-only) | HTTP client |
| nlohmann/json | request/response serialisation (already an engine dependency) |

Enable and point CMake at the header:

```bash
cmake -S . -B build -DAGENT_ENABLE_VLLM=ON -DHTTPLIB_INCLUDE_DIR=/path/to/httplib
```

This defines the **`AGENT_HAS_VLLM`** macro, which lets [`am_create`](c_api.md) construct the backend from JSON config.

---

## Construction

```cpp
AIModelVLLM(const std::string& base_url,
            const std::string& model_name,
            const std::string& api_key          = "",
            const std::string& embed_model_name = "",
            int                timeout_seconds   = 120,
            int                max_context       = 8192);
```

| Parameter | Default | Description |
|---|---|---|
| `base_url` | — | Server root, e.g. `http://localhost:8000` |
| `model_name` | — | Model id sent in every generation request |
| `api_key` | `""` | If non-empty, sent as `Authorization: Bearer <key>` |
| `embed_model_name` | `""` | Separate embedding model; falls back to `model_name` when empty |
| `timeout_seconds` | `120` | Applied to connection, read, and write timeouts |
| `max_context` | `8192` | Reported by `GetMaxContextLength()` |

The constructor builds a `httplib::Client` for `base_url` and sets all three timeouts.

---

## HTTP Endpoints

| Method | HTTP call | Request body | Response field read |
|---|---|---|---|
| `RawGenerate` | `POST /v1/completions` | `{model, prompt, temperature, max_tokens}` | `choices[0].text` |
| `RawEmbed` | `POST /v1/embeddings` | `{model, input}` | `data[0].embedding` |
| `ChatCompletion` | `POST /v1/chat/completions` | `{model, messages, temperature, max_tokens}` | `choices[0].message.content` |

All requests carry `Content-Type: application/json` and the optional bearer header from `AuthHeaders()`.

### Error handling

Each method throws `std::runtime_error` when:
- the HTTP request fails entirely (no response object), or
- the status is not `200` (the message includes the status code and response body), or
- the response JSON is malformed / missing the expected field.

These propagate up to the validated `AIModel::Generate` / `Embed`, and ultimately to [`AIModelLLMClient::complete`](ai_model_llm_client.md), which converts them into `Response{success=false}` so an agent stage can inspect the failure instead of crashing.

---

## Public Extras

### `ChatCompletion`

```cpp
std::string ChatCompletion(const nlohmann::json& messages,
                           float temperature, int max_tokens);
```

Direct access to the `/v1/chat/completions` endpoint with a caller-supplied `messages` array (role/content objects). Useful when you want proper chat-template formatting rather than the raw `/v1/completions` path that `RawGenerate` uses.

> Note: the [`AIModelLLMClient`](ai_model_llm_client.md) adapter calls `Generate` (→ `RawGenerate` → `/v1/completions`), *not* `ChatCompletion`, because the agent engine folds its own system+user prompt into a single string. If you need chat-template behaviour, that is the seam to customise.

### `AuthHeaders`

```cpp
httplib::Headers AuthHeaders() const;
```

Returns `{{"Authorization", "Bearer <api_key>"}}` when an API key was supplied, otherwise an empty header set.

---

## Identity

```cpp
std::string GetModelName()        const override;  // returns model_name
int         GetMaxContextLength() const override;  // returns max_context
```

---

## Usage

### Direct C++

```cpp
#include "ai_model/aimodel_vllm.hpp"
#include "agent/ai_model_llm_client.hpp"

AIModelVLLM model("http://localhost:8000",
                  "meta-llama/Llama-3-8b-instruct");
auto llm = std::make_shared<agent::AIModelLLMClient>(model);
AgentManager manager(config, llm);
```

### Via the C ABI (`am_create`)

```json
{
  "thread_pool_size": 16,
  "llm": {
    "backend":     "vllm",
    "base_url":    "http://localhost:8000",
    "model":       "meta-llama/Llama-3-8b-instruct",
    "api_key":     "",
    "embed_model": "BAAI/bge-m3",
    "max_context": 8192
  }
}
```

`am_create` reads this block and constructs `AIModelVLLM` (wrapped in an owning [`AIModelLLMClient`](ai_model_llm_client.md)) **only** when the engine was built with `AGENT_ENABLE_VLLM`. Otherwise it returns `AM_ERROR_INTERNAL` with an explanatory `am_last_error`, or — if no `llm` block is present — falls back to the mock stub.

---

## Thread-Safety

`httplib::Client` serialises requests on a single connection. For high concurrency across many agents, construct one `AIModelVLLM` per worker or place a connection pool in front. The `AIModel` embedding cache is the only shared mutable state and is safe for the read-mostly access pattern of `Search`.

---

## Related Components

- [AIModel](ai_model.md) — abstract base this implements
- [AIModelLlama](ai_model_llama.md) — local-model sibling backend
- [AIModelLLMClient](ai_model_llm_client.md) — adapts this to `agent::LLMClient`
- [AIModelMemoryBackend](ai_model_memory_backend.md) — adapts this to `agent::MemoryBackend`
- [C ABI](c_api.md) — `am_create` constructs this from config
