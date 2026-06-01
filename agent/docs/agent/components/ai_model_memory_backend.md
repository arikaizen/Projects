# AIModelMemoryBackend

`include/agent/ai_model_memory_backend.hpp` · `src/agent/ai_model_memory_backend.cpp`

## Overview

`AIModelMemoryBackend` is an adapter that backs the engine's `MemoryBackend` with an `AIModel`'s embedding and semantic-search capability. Entries are stored in an in-process `std::map`; semantic ranking is delegated to `AIModel::Search` (which embeds the query and all stored texts and returns the top-k by cosine similarity).

## Design Notes

- **In-memory store**: The store is a `std::map<std::string, Record>` guarded by a mutex. It does not persist across process restarts. For persistence, add serialisation to `write` and `remove` or replace the store with a database client.
- **Lock-free search**: `search` snapshots the store contents under lock, then releases the lock before calling `AIModel::Search` (which may be slow — it embeds every stored text). This prevents the mutex from being held during blocking embedding work.
- **Embedding cache**: `AIModel::Embed` caches results by input text, so repeated searches over the same stored content are fast.
- **External model ownership**: The adapter holds a non-owning reference to an `AIModel`. The model must outlive the backend (and therefore the `AgentManager`).

## Construction

```cpp
explicit AIModelMemoryBackend(AIModel& model);
```

## Interface

```cpp
void write(const std::string& id, const std::string& content,
           const nlohmann::json& metadata = {}) override;
std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) override;
std::vector<MemoryEntry> list(const std::string& filter = "") override;
void remove(const std::string& id) override;
```

### `write`

Acquires `m_mutex` and upserts `{content, metadata}` under `id`.

### `search`

1. Acquires `m_mutex` and takes a snapshot of the store.
2. Releases `m_mutex`.
3. Builds `labels` and `texts` vectors from the snapshot.
4. Calls `m_model.Search(query, labels, texts, top_k)` which embeds query and texts, ranks by cosine similarity, and returns `(score, label)` pairs.
5. Constructs `MemoryEntry` objects from the results.

### `list`

Acquires `m_mutex`, iterates the store, and returns all entries whose id or content contains `filter` as a substring. Returns all entries when `filter` is empty.

### `remove`

Acquires `m_mutex` and erases the entry for `id`. Also calls `ClearEmbedCache` to remove stale cached embeddings (since the text no longer exists).

## Internal Structure

```cpp
struct Record {
    std::string    content;
    nlohmann::json metadata;
};
AIModel&                        m_model;
mutable std::mutex              m_mutex;
std::map<std::string, Record>   m_store;
```

## Example

```cpp
AIModelVLLM model("http://localhost:8000", "llama-3.1-8b",
                   /*api_key=*/"", "embed-model-v2");
auto memory = std::make_shared<AIModelMemoryBackend>(model);
auto mgr    = AgentManager(config, llm, memory);
```

After wiring, `MemoryWriteAction` / `MemoryReadAction` in agent plans use real semantic search.

## Related Components

- [`MemoryBackend`](memory_backend.md) — the interface this class implements
- [`AIModel`](ai_model.md) — the model hierarchy this class wraps
- [`AIModelLLMClient`](ai_model_llm_client.md) — companion adapter for LLM calls
- [`MemoryWriteAction`](memory_actions.md), [`MemoryReadAction`](memory_actions.md), [`MemoryListAction`](memory_actions.md) — action-layer consumers
