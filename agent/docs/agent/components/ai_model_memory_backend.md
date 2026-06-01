# AIModelMemoryBackend

`include/agent/ai_model_memory_backend.hpp` · `src/agent/ai_model_memory_backend.cpp`

---

## Overview

`AIModelMemoryBackend` adapts an [`AIModel`](ai_model.md)'s embedding + semantic-search capability to the engine's [`MemoryBackend`](llm_client.md) interface. It replaces the `NoOpMemoryBackend` stub with a working semantic memory store — backed by the same `AIModel` that powers the LLM client.

```
AIModel::Embed / AIModel::Search
        ▲
        │  AIModelMemoryBackend::search()
        │
agent::MemoryBackend  ──► used by MemoryWriteAction / MemoryReadAction / MemoryListAction
```

---

## How It Works

The adapter keeps an in-memory store of records (`id → {content, metadata}`). On `search`, it:

1. Snapshots the store under a mutex.
2. Hands every stored `(label=id, text=content)` plus the query to `AIModel::Search`, which embeds them all (using the model's embedding cache) and ranks by cosine similarity.
3. Maps the ranked `(score, id)` pairs back to `MemoryEntry` objects with their original content and metadata.

The embedding work happens **outside** the lock, so concurrent agents are not serialised on slow embedding calls.

---

## API

```cpp
explicit AIModelMemoryBackend(AIModel& model);

void write(const std::string& id, const std::string& content,
           const nlohmann::json& metadata = {}) override;
std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) override;
std::vector<MemoryEntry> list(const std::string& filter = "") override;
void remove(const std::string& id) override;
```

### `write`

Inserts or replaces the record under `id`. Thread-safe.

### `search`

Returns the top-`k` records most semantically similar to `query`, each with a cosine-similarity `score`. Returns an empty vector if the store is empty or `query` is empty.

```cpp
auto hits = mem.search("how is the build configured", 3);
// hits[0].id, hits[0].content, hits[0].metadata, hits[0].score
```

### `list`

Returns all records, or those whose **id or content** contains `filter` as a substring. `score` is `0.0` (no ranking performed).

### `remove`

Deletes the record under `id`. No-op if absent.

---

## MemoryEntry

```cpp
struct MemoryEntry {
    std::string    id;
    std::string    content;
    nlohmann::json metadata;
    float          score;   // cosine similarity from search(); 0.0 from list()
};
```

---

## Thread-Safety

The internal `std::map` store is guarded by a `std::mutex`. `search` copies a snapshot under the lock and runs the embedding/ranking afterwards without holding it. `AIModel::Embed` is assumed thread-safe per the `AIModel` contract (its only shared state is the embedding cache).

---

## Usage

```cpp
#include "agent/ai_model_memory_backend.hpp"
#include "agent/ai_model_llm_client.hpp"
#include "ai_model/aimodel_vllm.hpp"   // requires AGENT_ENABLE_VLLM

// One model powers both the LLM client and memory:
auto shared_model = std::make_shared<AIModelVLLM>("http://localhost:8000", "my-model");
auto llm = std::make_shared<agent::AIModelLLMClient>(*shared_model);
auto mem = std::make_shared<agent::AIModelMemoryBackend>(*shared_model);

AgentManager manager(config, llm, mem);
```

> The model must outlive both adapters. Hold it in a `shared_ptr` (as above) or use the owning `AIModelLLMClient(unique_ptr<AIModel>)` constructor for the LLM side and keep a separate owned model for memory.

Agents then use it through the memory actions:

```json
{"name": "MemoryWriteAction", "id": "m1", "inputs": {"id": "note1", "content": "..."}}
{"name": "MemoryReadAction",  "id": "m2", "inputs": {"query": "...", "top_k": 5}}
```

See [`actions.md`](actions.md).

---

## Replaces

| Stub | Replaced by |
|---|---|
| `NoOpMemoryBackend` (default) | `AIModelMemoryBackend` wrapping a real `AIModel` |

---

## Related Components

- [`AIModel`](ai_model.md) — supplies `Embed` / `Search`
- [`AIModelLLMClient`](ai_model_llm_client.md) — sibling adapter; can share the same model
- [`LLMClient & MemoryBackend`](llm_client.md) — the `MemoryBackend` interface implemented here
- [`Actions`](actions.md) — `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction`
- [`AgentContext`](agent.md) — exposes `memory()` to actions
