# AIModelLlama

`third_party/ai_model/aimodel_llama.hpp` · `third_party/ai_model/aimodel_llama.cpp`

> A concrete [`AIModel`](ai_model.md) backend. **Opt-in** — built only with `-DAGENT_ENABLE_LLAMA=ON`.

---

## Overview

`AIModelLlama` runs a **local GGUF model in-process** via [llama.cpp](https://github.com/ggerganov/llama.cpp). It implements the [`AIModel`](ai_model.md) contract by loading the model, managing an inference context, and driving llama.cpp's tokenizer / sampler / decode loop directly. With a CUDA-enabled llama.cpp build it runs on GPU.

This backend is the choice for fully offline / on-device deployments where no HTTP server is involved.

---

## Dependencies

| Dependency | Why |
|---|---|
| `llama.h` + llama.cpp libs (`llama`, `ggml`, `ggml-base`, …) | model load, tokenize, decode, sample, embed |
| (optional) CUDA + `ggml-cuda` | GPU acceleration |

Enable and point CMake at a built llama.cpp tree:

```bash
cmake -S . -B build -DAGENT_ENABLE_LLAMA=ON -DLLAMA_DIR=/path/to/llama.cpp
```

CMake adds `${LLAMA_DIR}/include` and `${LLAMA_DIR}/ggml/include` to the include path, links `${LLAMA_DIR}/build/bin`, and defines the **`AGENT_HAS_LLAMA`** macro.

---

## Construction & Lifetime

```cpp
AIModelLlama(const std::string& model_path,
             int context_size = 4096, int thread_count = 4);
~AIModelLlama() override;
```

| Parameter | Default | Description |
|---|---|---|
| `model_path` | — | Path to a `.gguf` model file |
| `context_size` | `4096` | Context window (`n_ctx`); also reported by `GetMaxContextLength()` |
| `thread_count` | `4` | CPU threads for prompt + batch decode |

The constructor:
1. `llama_backend_init()`.
2. Loads the model with `llama_model_load_from_file` — throws `std::runtime_error` if the file cannot be loaded.
3. Resolves the model name via `llama_model_desc`, falling back to the file stem.
4. Creates an inference context sized to `context_size` with the requested thread counts. On failure it frees the model and throws.

The destructor frees the inference context, the model, and calls `llama_backend_free()` — so the object owns its full llama.cpp lifecycle (this is why `AIModel` is non-copyable/non-movable).

---

## Generation — `RawGenerate`

```cpp
std::string RawGenerate(const std::string& prompt, float t, int max) override;
```

Stateless per call:

1. Tokenize the prompt (`add_bos = true`).
2. **Clear KV cache** (`llama_memory_clear`) so no state leaks from a previous call — each `Generate` is independent (matching the agent engine's "the engine owns history" model).
3. Evaluate the prompt with `llama_decode`.
4. Build a sampler chain: **temperature `t` → top-p (0.9) → distribution sampler** (`LLAMA_DEFAULT_SEED`).
5. Greedily decode up to `max` tokens, appending each detokenized piece; stop early on an end-of-generation token (`llama_vocab_is_eog`).
6. Free the sampler and return the accumulated text.

`llama_decode` failures throw `std::runtime_error` (the sampler is freed first to avoid a leak).

---

## Embeddings — `RawEmbed`

```cpp
std::vector<float> RawEmbed(const std::string& text) override;
```

Creates a **dedicated embedding context** (`embeddings = true`, mean pooling, `n_ctx = 512`) separate from the generation context, tokenizes the text (`add_bos = false`), decodes, and reads the sequence embedding via `llama_get_embeddings_seq`. Returns a vector of length `llama_model_n_embd`.

Throws `std::runtime_error` if the context can't be created, decode fails, or the model produces no embedding output (i.e. the GGUF doesn't support embeddings).

> The embedding context is created and freed **per call**. For embedding-heavy workloads (large [`AIModelMemoryBackend`](ai_model_memory_backend.md) searches) this is a known cost; the base-class embedding cache mitigates repeats.

---

## Tokenizer Access

```cpp
std::vector<llama_token> Tokenize(const std::string& text, bool add_bos) const;
```

Public helper used by both `RawGenerate` and `RawEmbed`. It calls `llama_tokenize` twice — first to size the output (llama.cpp returns the negative count when the buffer is too small), then to fill it.

Additional accessors:

```cpp
llama_model* ModelPtr()    const noexcept;
int          CtxSize()     const noexcept;
int          ThreadCount() const noexcept;
```

---

## Identity

```cpp
std::string GetModelName()        const override;  // from llama_model_desc / file stem
int         GetMaxContextLength() const override;  // context_size
```

---

## Usage

```cpp
#include "ai_model/aimodel_llama.hpp"
#include "agent/ai_model_llm_client.hpp"
#include "agent/ai_model_memory_backend.hpp"

AIModelLlama model("models/llama-3.1-8b-instruct.Q4_K_M.gguf",
                   /*context_size=*/4096, /*thread_count=*/8);

auto llm = std::make_shared<agent::AIModelLLMClient>(model);     // generation
auto mem = std::make_shared<agent::AIModelMemoryBackend>(model); // embeddings
AgentManager manager(config, llm, mem);   // one model backs both
```

> Wiring `AIModelLlama` into `am_create` is not done by default (it needs a `model_path` and there's no server URL). The straightforward extension is a `"backend":"llama"` branch in `am_create`, mirroring the vLLM branch, guarded by `#ifdef AGENT_HAS_LLAMA`.

---

## Thread-Safety

A single `llama_context` is **not** safe for concurrent decode. Because the agent engine may run several agents in parallel, either:
- construct one `AIModelLlama` per concurrent worker, or
- serialise calls behind a mutex in a thin wrapper.

The generation path clears the KV cache each call, so sequential calls on one instance are safe and independent.

---

## Related Components

- [AIModel](ai_model.md) — abstract base this implements
- [AIModelVLLM](ai_model_vllm.md) — HTTP sibling backend
- [AIModelLLMClient](ai_model_llm_client.md) — adapts this to `agent::LLMClient`
- [AIModelMemoryBackend](ai_model_memory_backend.md) — adapts this to `agent::MemoryBackend`
- [C ABI](c_api.md) — extend `am_create` with a `"backend":"llama"` branch to construct this
