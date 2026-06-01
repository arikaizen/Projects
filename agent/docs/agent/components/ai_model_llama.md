# AIModelLlama

`third_party/ai_model/aimodel_llama.hpp` · `third_party/ai_model/aimodel_llama.cpp`

## Overview

`AIModelLlama` implements `AIModel` for a local GGUF model via **llama.cpp**. It loads the model and a stateless inference context at construction time and exposes generation and embedding through the standard `AIModel` interface.

**Requires:** CMake option `-DAGENT_ENABLE_LLAMA=ON` and `llama.h` on the include path.

## Construction

```cpp
AIModelLlama(const std::string& model_path,
             int context_size = 4096,
             int thread_count = 4);
```

| Parameter | Default | Description |
|---|---|---|
| `model_path` | — | Path to the GGUF model file |
| `context_size` | `4096` | KV cache context size in tokens |
| `thread_count` | `4` | Number of threads for llama.cpp inference |

Loads the model via `llama_load_model_from_file` and creates an inference context. Throws if the model file cannot be loaded.

## Destructor

```cpp
~AIModelLlama() override;
```

Frees the inference context and model via `llama_free` / `llama_free_model`.

## `RawGenerate`

Implements single-turn text completion:

1. Tokenizes the prompt via `llama_tokenize`.
2. Clears KV cache via `llama_kv_cache_clear` (stateless — no conversation state is preserved between calls).
3. Decodes tokens in batches.
4. Samples output using a temperature → top-p → distribution sampler chain.
5. Stops at EOS token or `max_tokens` limit.
6. Decodes the output token sequence back to a UTF-8 string.

## `RawEmbed`

Computes an embedding vector for the input text using the same llama context. Results are cached by `AIModel::Embed`.

## Additional Accessors

```cpp
llama_model* ModelPtr()    const noexcept;
int          CtxSize()     const noexcept;
int          ThreadCount() const noexcept;

std::vector<llama_token> Tokenize(const std::string& text, bool add_bos) const;
```

Exposed for advanced integrations (e.g. batched generation pipelines).

## CMake Build

```cmake
cmake -DAGENT_ENABLE_LLAMA=ON ..
```

This adds `aimodel_llama.cpp` to the build, finds `llama.h` and links `libllama`, and defines the `AGENT_HAS_LLAMA` macro.

## Example Usage

```cpp
auto model = std::make_unique<AIModelLlama>(
    "/models/llama-3.1-8b-instruct-q4.gguf", 4096, 8);
auto llm = std::make_shared<AIModelLLMClient>(std::move(model));
auto mgr = AgentManager(config, llm);
```

## Notes

- Each `RawGenerate` call clears the KV cache, making calls independent (stateless). The agent engine manages conversation state in `AgentContext::history`, so this is the correct design.
- For production use, consider enabling GPU offloading via llama.cpp's `n_gpu_layers` parameter (expose via the constructor or a setter).
- The embedding vector dimension depends on the model architecture.

## Related Components

- [`AIModel`](ai_model.md) — abstract base class
- [`AIModelLLMClient`](ai_model_llm_client.md) — uses `Generate` for LLM calls
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — uses `Embed` / `Search` for memory
- [`AIModelVLLM`](ai_model_vllm.md) — alternative remote backend
