# AIModel (abstract base)

`third_party/ai_model/aimodel.hpp` · `third_party/ai_model/aimodel.cpp`

## Overview

`AIModel` is the abstract base class for all local and remote model backends. It provides a unified interface for text generation, embedding, similarity computation, and semantic search, with a built-in embedding cache.

The agent engine does not depend on `AIModel` directly — it uses the abstract [`LLMClient`](llm_client.md) and [`MemoryBackend`](memory_backend.md) interfaces. The two adapter classes [`AIModelLLMClient`](ai_model_llm_client.md) and [`AIModelMemoryBackend`](ai_model_memory_backend.md) bridge `AIModel` into those abstractions.

## Interface

```cpp
class AIModel {
public:
    // Non-virtual public methods (caching layer on top of raw virtuals)
    std::string Generate(const std::string& prompt,
                         float temperature = 0.7f, int max_tokens = 512);
    std::vector<float> Embed(const std::string& text, bool use_cache = true);
    float Similarity(const std::string& a, const std::string& b);
    std::vector<std::pair<float, std::string>> Search(
        const std::string& query,
        const std::vector<std::string>& labels,
        const std::vector<std::string>& texts,
        int top_n = 3);

    void ClearEmbedCache() noexcept { m_embedding_cache.clear(); }

    virtual std::string GetModelName()        const = 0;
    virtual int         GetMaxContextLength() const = 0;

protected:
    // Subclasses implement these two methods
    virtual std::string        RawGenerate(const std::string& prompt, float t, int max) = 0;
    virtual std::vector<float> RawEmbed(const std::string& text) = 0;

    std::unordered_map<std::string, std::vector<float>> m_embedding_cache;
};
```

## Methods

### `Generate`

```cpp
std::string Generate(const std::string& prompt, float temperature = 0.7f, int max_tokens = 512);
```

Calls `RawGenerate`. The public method exists as a hook point for future pre/post-processing. No caching is applied to generation results.

### `Embed`

```cpp
std::vector<float> Embed(const std::string& text, bool use_cache = true);
```

Returns the embedding vector for `text`. When `use_cache=true` (default), checks `m_embedding_cache` first and calls `RawEmbed` only on a cache miss. The cache is an `std::unordered_map<std::string, std::vector<float>>` keyed by the exact input string.

### `Similarity`

```cpp
float Similarity(const std::string& a, const std::string& b);
```

Computes the cosine similarity between `Embed(a)` and `Embed(b)`. Returns a value in `[-1, 1]`. Both embeddings are cached.

### `Search`

```cpp
std::vector<std::pair<float, std::string>> Search(
    const std::string& query,
    const std::vector<std::string>& labels,
    const std::vector<std::string>& texts,
    int top_n = 3);
```

Embeds `query` and all `texts`, computes cosine similarity for each, and returns the top-n `(score, label)` pairs sorted by descending score. Used by `AIModelMemoryBackend::search`.

### `ClearEmbedCache`

```cpp
void ClearEmbedCache() noexcept;
```

Empties the embedding cache. Useful when memory is constrained or after a model switch.

## Types Helper

`third_party/ai_model/types.hpp` defines:

```cpp
enum class Role : uint8_t { System, User, Assistant };
struct Message { Role role; std::string content; };
std::string RoleToStr(Role r);
Role        RoleFromStr(const std::string& s);
```

These are used by chat-completion implementations.

## Non-Copyable, Non-Movable

`AIModel` deletes copy/move constructors and assignment operators. Models are always heap-allocated and passed by pointer or `unique_ptr`.

## Concrete Subclasses

| Class | Description | Doc |
|---|---|---|
| `AIModelVLLM` | Remote vLLM / OpenAI-compatible server | [ai_model_vllm.md](ai_model_vllm.md) |
| `AIModelLlama` | Local GGUF via llama.cpp | [ai_model_llama.md](ai_model_llama.md) |

## Adapters

| Adapter | Bridges to |
|---|---|
| `AIModelLLMClient` | `AIModel::Generate` → `LLMClient::complete` |
| `AIModelMemoryBackend` | `AIModel::Embed` + `AIModel::Search` → `MemoryBackend` |

## Related Components

- [`AIModelLLMClient`](ai_model_llm_client.md) — `LLMClient` adapter
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — `MemoryBackend` adapter
- [`AIModelVLLM`](ai_model_vllm.md) — remote HTTP backend
- [`AIModelLlama`](ai_model_llama.md) — local llama.cpp backend
