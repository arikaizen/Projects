# AIModel Hierarchy

`third_party/ai_model/` — copied from the project's `AI/convo/` module

---

## Overview

`AIModel` is an abstract base class for a language model that can **generate text** and **produce embeddings**. It was lifted from the project's `AI/convo/` library into the agent engine's `third_party/ai_model/` so the engine can run on real models through the [`AIModelLLMClient`](ai_model_llm_client.md) and [`AIModelMemoryBackend`](ai_model_memory_backend.md) adapters.

The class lives in the **global namespace** (not `namespace agent`) because it is third-party code preserved verbatim.

```
AIModel  (abstract — generation + embeddings + validation)
├── AIModelLlama   (llama.cpp / local GGUF; opt-in)
└── AIModelVLLM    (HTTP / OpenAI-compatible API; opt-in)
```

---

## Files

| File | Contents | Built by default? |
|---|---|---|
| `types.hpp` | `Role`, `Message`, `RoleToStr`, `RoleFromStr` | header only |
| `aimodel.hpp` / `aimodel.cpp` | abstract `AIModel` base | ✅ always |
| `aimodel_vllm.hpp` / `aimodel_vllm.cpp` | `AIModelVLLM` HTTP backend | ⛔ only with `-DAGENT_ENABLE_VLLM=ON` |
| `aimodel_llama.hpp` / `aimodel_llama.cpp` | `AIModelLlama` local backend | ⛔ only with `-DAGENT_ENABLE_LLAMA=ON` |

The abstract base (`aimodel.cpp`) compiles with only the C++ standard library, so it — and the adapters built on it — are always part of `agent_core`. The concrete backends pull in heavy dependencies (cpp-httplib, llama.cpp, CUDA) and are therefore behind CMake options that default **OFF**.

---

## Abstract Interface

```cpp
class AIModel {
public:
    // Public, validated entry points (defined in aimodel.cpp):
    std::string Generate(const std::string& prompt,
                         float temperature = 0.7f, int max_tokens = 512);
    std::vector<float> Embed(const std::string& text, bool use_cache = true);
    float Similarity(const std::string& a, const std::string& b);
    std::vector<std::pair<float, std::string>> Search(
        const std::string& query,
        const std::vector<std::string>& labels,
        const std::vector<std::string>& texts,
        int top_n = 3);

    void ClearEmbedCache() noexcept;

    virtual std::string GetModelName()        const = 0;
    virtual int         GetMaxContextLength() const = 0;

protected:
    // Backends implement these two raw operations:
    virtual std::string        RawGenerate(const std::string& prompt, float t, int max) = 0;
    virtual std::vector<float> RawEmbed(const std::string& text) = 0;

    std::unordered_map<std::string, std::vector<float>> m_embedding_cache;
};
```

### Validation (in the public methods)

The base class validates inputs before delegating to the `Raw*` virtuals:

| Method | Throws `std::invalid_argument` / `std::runtime_error` when |
|---|---|
| `Generate` | prompt blank; temperature ∉ [0, 2]; max_tokens < 1; result blank |
| `Embed` | text blank; embedding has zero magnitude |
| `Similarity` | either text blank |
| `Search` | query blank; `labels.size() != texts.size()`; `top_n < 1` |

These exceptions are what the adapters catch and convert into engine-friendly results (`Response{success=false}`).

### Built-in semantic search

`Search` embeds the query and every candidate text (using the cache), ranks by cosine similarity, and returns the top-N `(score, label)` pairs. This is exactly what [`AIModelMemoryBackend`](ai_model_memory_backend.md) delegates to.

### Embedding cache

`Embed(text, use_cache=true)` memoises results in `m_embedding_cache`, so repeated embeds of the same text (common during `Search`) are free. `ClearEmbedCache()` empties it.

---

## AIModelVLLM (opt-in)

HTTP backend for any vLLM / OpenAI-compatible server.

```cpp
AIModelVLLM(const std::string& base_url,
            const std::string& model_name,
            const std::string& api_key = "",
            const std::string& embed_model_name = "",
            int timeout_seconds = 120,
            int max_context = 8192);
```

| Endpoint | Method |
|---|---|
| `POST /v1/completions` | `RawGenerate` |
| `POST /v1/embeddings` | `RawEmbed` |
| `POST /v1/chat/completions` | `ChatCompletion` (direct access) |

Requires `httplib.h` (header-only cpp-httplib) + nlohmann/json. Enable with:

```bash
cmake -S . -B build -DAGENT_ENABLE_VLLM=ON -DHTTPLIB_INCLUDE_DIR=/path/to/httplib
```

This also defines the `AGENT_HAS_VLLM` macro, which lets `am_create` construct it from config.

---

## AIModelLlama (opt-in)

Local GGUF backend via llama.cpp.

```cpp
AIModelLlama(const std::string& model_path,
             int context_size = 4096, int thread_count = 4);
```

Requires `llama.h` and the llama.cpp libraries (optionally CUDA). Enable with:

```bash
cmake -S . -B build -DAGENT_ENABLE_LLAMA=ON -DLLAMA_DIR=/path/to/llama.cpp
```

This defines the `AGENT_HAS_LLAMA` macro.

---

## How It Plugs Into the Engine

```
AIModel (this hierarchy)
   │
   ├── AIModelLLMClient  ─────►  agent::LLMClient  ──►  used by all Stages
   │     (Generate)
   │
   └── AIModelMemoryBackend ──►  agent::MemoryBackend ► used by Memory*Action
         (Embed / Search)
```

A single `AIModel` instance can back **both** adapters at once — one model powers reasoning *and* memory.

---

## Why `AIConvo` Was Not Copied

The `AI/convo/` module also contains `AIConvo` (a stateful conversation layer with its own history). It was deliberately **not** brought over: the agent engine already owns conversation state in `AgentContext::history`, and every `Stage` builds a fully-rendered prompt per call. Using `AIConvo` would duplicate that history. The stateless `AIModel::Generate` layer is the correct integration point. See [`AIModelLLMClient`](ai_model_llm_client.md) for the rationale.

---

## Extending to a New Model

Adding a backend is purely additive — nothing in the agent engine changes:

```cpp
class AIModelOllama : public AIModel {
    std::string GetModelName()        const override { return "llama3"; }
    int         GetMaxContextLength() const override { return 8192; }
protected:
    std::string        RawGenerate(const std::string&, float, int) override;
    std::vector<float> RawEmbed(const std::string&) override;
};
```

Then `AIModelLLMClient(model)` and `AIModelMemoryBackend(model)` work unchanged.

---

## Related Components

- [`AIModelLLMClient`](ai_model_llm_client.md) — `AIModel` → `LLMClient` adapter
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — `AIModel` → `MemoryBackend` adapter
- [`LLMClient & MemoryBackend`](llm_client.md) — the engine interfaces being adapted to
- [`AgentManager`](agent_manager.md) — receives the adapted client/backend at construction
- [C ABI](c_api.md) — `am_create` can construct `AIModelVLLM` when `AGENT_HAS_VLLM` is defined
