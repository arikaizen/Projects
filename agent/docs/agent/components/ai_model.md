# AIModel (abstract base)

`third_party/ai_model/aimodel.hpp` · `third_party/ai_model/aimodel.cpp`

> Part of the model integration set. See also:
> [AIModelVLLM](ai_model_vllm.md) · [AIModelLlama](ai_model_llama.md) ·
> [AIModelLLMClient](ai_model_llm_client.md) · [AIModelMemoryBackend](ai_model_memory_backend.md)

---

## Overview

`AIModel` is the abstract base class for a language model that can **generate text** and **produce embeddings**. It was copied from the project's `AI/convo/` library into `agent/third_party/ai_model/` so the agent engine can run on real models through two adapters. It lives in the **global namespace** (it is third-party code preserved verbatim, not `namespace agent`).

The design splits each capability into a **public validated method** (in the base) and a **protected raw virtual** (implemented by each backend). The base never talks to a model directly — it validates inputs, manages the embedding cache, computes cosine similarity, and delegates the actual work to `RawGenerate` / `RawEmbed`.

```
                    ┌─────────────────────────────────────────┐
   caller ───►      │  AIModel  (validation + cache + ranking) │
                    │     Generate / Embed / Similarity/Search │
                    └───────────────┬──────────────┬───────────┘
                                    │ RawGenerate  │ RawEmbed   (pure virtual)
                    ┌───────────────▼──────────────▼───────────┐
                    │  AIModelVLLM        │  AIModelLlama       │
                    └─────────────────────┴─────────────────────┘
```

---

## Class Definition

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

    // Backend identity:
    virtual std::string GetModelName()        const = 0;
    virtual int         GetMaxContextLength() const = 0;

protected:
    AIModel() = default;
    // Backends implement these two raw operations:
    virtual std::string        RawGenerate(const std::string& prompt, float t, int max) = 0;
    virtual std::vector<float> RawEmbed(const std::string& text) = 0;

    std::unordered_map<std::string, std::vector<float>> m_embedding_cache;
};
```

The class is **non-copyable and non-movable** (the deleted special members reflect that a model owns heavyweight, non-trivially-copyable resources such as a loaded GGUF model or an HTTP client).

---

## Public Methods (defined in `aimodel.cpp`)

### `Generate`

```cpp
std::string Generate(const std::string& prompt, float temperature = 0.7f, int max_tokens = 512);
```

Validates, then calls the backend's `RawGenerate`.

| Check | Exception |
|---|---|
| `prompt` blank (only whitespace) | `std::invalid_argument` |
| `temperature` ∉ [0.0, 2.0] | `std::invalid_argument` |
| `max_tokens` < 1 | `std::invalid_argument` |
| result blank | `std::runtime_error` |

These are exactly the exceptions [`AIModelLLMClient`](ai_model_llm_client.md) catches and turns into `Response{success=false}`.

### `Embed`

```cpp
std::vector<float> Embed(const std::string& text, bool use_cache = true);
```

Returns the embedding vector for `text`. With `use_cache=true` (default), the result is memoised in `m_embedding_cache` keyed by the exact text, so repeated embeds (very common during `Search`) are free.

| Check | Exception |
|---|---|
| `text` blank | `std::invalid_argument` |
| embedding has zero magnitude | `std::runtime_error` ("does this model support embeddings?") |

### `Similarity`

```cpp
float Similarity(const std::string& a, const std::string& b);
```

Cosine similarity of `Embed(a)` and `Embed(b)`. Returns a value in `[-1, 1]` (typically `[0, 1]` for text embeddings). Throws if either text is blank.

### `Search`

```cpp
std::vector<std::pair<float, std::string>> Search(
    const std::string& query,
    const std::vector<std::string>& labels,
    const std::vector<std::string>& texts,
    int top_n = 3);
```

Embeds `query` and every `texts[i]`, computes cosine similarity to each, sorts descending, and returns the top-`N` `(score, label)` pairs. This is the engine for [`AIModelMemoryBackend::search`](ai_model_memory_backend.md).

| Check | Exception |
|---|---|
| `query` blank | `std::invalid_argument` |
| `labels.size() != texts.size()` | `std::invalid_argument` |
| `top_n` < 1 | `std::invalid_argument` |

### `ClearEmbedCache`

```cpp
void ClearEmbedCache() noexcept;
```

Empties `m_embedding_cache`. Call after a large `Search` over transient documents if you do not want them retained.

---

## Internal Math (file-local helpers in `aimodel.cpp`)

| Helper | Purpose |
|---|---|
| `IsBlank(s)` | true if `s` is empty or all whitespace |
| `VecNorm(v)` | Euclidean norm `√(Σ vᵢ²)` |
| `CosineSim(a, b)` | `dot(a,b) / (‖a‖·‖b‖)`; returns 0 if either norm is 0 |

> **Historical note:** the original `AI/convo` code had a bug — `if (VecNorm(result) == 0.0f){}` with empty braces made an embedding-error throw unconditional. The copied version carries the fix (`if (...) { throw ...; }`), as documented in `AI/REFACTOR.md`.

---

## Pure-Virtual Contract (implemented by backends)

```cpp
virtual std::string        RawGenerate(const std::string& prompt, float t, int max) = 0;
virtual std::vector<float> RawEmbed(const std::string& text) = 0;
virtual std::string        GetModelName()        const = 0;
virtual int                GetMaxContextLength() const = 0;
```

A backend implements these four. `RawGenerate`/`RawEmbed` receive **already-validated** inputs (non-blank prompt, sane temperature/tokens), so they can focus purely on talking to the model.

---

## Build

`aimodel.cpp` depends only on the C++ standard library, so it is **always** compiled into `agent_core`. The concrete backends are opt-in:

```bash
cmake -S . -B build -DAGENT_ENABLE_VLLM=ON   # AIModelVLLM
cmake -S . -B build -DAGENT_ENABLE_LLAMA=ON  # AIModelLlama
```

---

## Writing a Custom Backend

```cpp
class AIModelEcho : public AIModel {
public:
    std::string GetModelName()        const override { return "echo"; }
    int         GetMaxContextLength() const override { return 4096; }
protected:
    std::string RawGenerate(const std::string& p, float, int) override { return "ECHO:" + p; }
    std::vector<float> RawEmbed(const std::string& t) override {
        std::vector<float> v(26, 0.f);
        for (unsigned char c : t) if (std::isalpha(c)) v[std::tolower(c) - 'a'] += 1.f;
        return v;   // must be non-zero for alpha text (base rejects zero vectors)
    }
};
```

The in-tree test `tests/agent/test_ai_model_adapter.cpp` uses exactly this pattern (a `FakeModel`) to verify both adapters with no external dependencies.

---

## Related Components

- [AIModelVLLM](ai_model_vllm.md) — HTTP / OpenAI-compatible backend
- [AIModelLlama](ai_model_llama.md) — local GGUF backend via llama.cpp
- [AIModelLLMClient](ai_model_llm_client.md) — adapts `Generate` to `agent::LLMClient`
- [AIModelMemoryBackend](ai_model_memory_backend.md) — adapts `Embed`/`Search` to `agent::MemoryBackend`
- [LLMClient & MemoryBackend](llm_client.md) — the engine interfaces being adapted to
