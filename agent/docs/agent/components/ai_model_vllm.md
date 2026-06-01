# AIModelVLLM

`third_party/ai_model/aimodel_vllm.hpp` · `third_party/ai_model/aimodel_vllm.cpp`

## Overview

`AIModelVLLM` implements `AIModel` for a remote vLLM server (or any OpenAI-compatible HTTP API). It communicates via `cpp-httplib` using three endpoints:

| Endpoint | Used for |
|---|---|
| `POST /v1/completions` | `RawGenerate` — single-turn text completion |
| `POST /v1/chat/completions` | `ChatCompletion` — multi-turn chat |
| `POST /v1/embeddings` | `RawEmbed` — text embedding |

**Requires:** CMake option `-DAGENT_ENABLE_VLLM=ON` and `httplib.h` on the include path.

## Construction

```cpp
AIModelVLLM(const std::string& base_url,
            const std::string& model_name,
            const std::string& api_key = "",
            const std::string& embed_model_name = "",
            int timeout_seconds = 120,
            int max_context = 8192);
```

| Parameter | Default | Description |
|---|---|---|
| `base_url` | — | Server base URL, e.g. `"http://localhost:8000"` |
| `model_name` | — | Model identifier sent in API requests |
| `api_key` | `""` | Bearer token for the `Authorization` header (empty = no auth) |
| `embed_model_name` | `""` | Model name for embedding requests; falls back to `model_name` if empty |
| `timeout_seconds` | `120` | HTTP request timeout |
| `max_context` | `8192` | Reported by `GetMaxContextLength()` |

## `RawGenerate`

Calls `POST /v1/completions` with:

```json
{
  "model": "<model_name>",
  "prompt": "<prompt>",
  "temperature": <t>,
  "max_tokens": <max>
}
```

Returns `response["choices"][0]["text"]`.

## `ChatCompletion`

```cpp
std::string ChatCompletion(const nlohmann::json& messages,
                           float temperature, int max_tokens);
```

Calls `POST /v1/chat/completions` with the provided messages array. Returns the assistant content from `choices[0].message.content`. Not called by `AIModelLLMClient` (which uses stateless `Generate`), but available for custom integrations.

## `RawEmbed`

Calls `POST /v1/embeddings` with:

```json
{ "model": "<embed_model_name>", "input": "<text>" }
```

Returns `data[0].embedding` as `std::vector<float>`. Results are cached by `AIModel::Embed`.

## `AuthHeaders`

```cpp
httplib::Headers AuthHeaders() const;
```

Returns `Authorization: Bearer <api_key>` if an API key was provided, otherwise empty headers.

## CMake Build

```cmake
cmake -DAGENT_ENABLE_VLLM=ON ..
```

This adds `aimodel_vllm.cpp` to the build, finds `httplib.h`, and defines the `AGENT_HAS_VLLM` macro. The C ABI (`am_create`) reads `config["llm"]["backend"] == "vllm"` to select this backend at runtime.

## Example Usage

```cpp
auto model = std::make_unique<AIModelVLLM>(
    "http://localhost:8000", "meta-llama/Llama-3.1-8B-Instruct");
auto llm = std::make_shared<AIModelLLMClient>(std::move(model));
auto mgr = AgentManager(config, llm);
```

Via C ABI:

```json
{
  "prompts_dir": "./prompts",
  "llm": {
    "backend": "vllm",
    "base_url": "http://localhost:8000",
    "model_name": "meta-llama/Llama-3.1-8B-Instruct"
  }
}
```

## Related Components

- [`AIModel`](ai_model.md) — abstract base class
- [`AIModelLLMClient`](ai_model_llm_client.md) — uses `Generate` for LLM calls
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — uses `Embed` / `Search` for memory
- [`C ABI`](c_api.md) — selects this backend via `config["llm"]["backend"] == "vllm"`
