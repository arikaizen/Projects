# MemoryBackend

`include/agent/memory_backend.hpp`

## Overview

`MemoryBackend` is the abstract interface for long-term agent memory. Implementations connect to a vector database, SQLite, or any other store. The default `NoOpMemoryBackend` compiles cleanly and allows all other engine functionality to work without a configured memory store.

## Interface

```cpp
class MemoryBackend {
public:
    virtual ~MemoryBackend() = default;

    virtual void write(const std::string& id, const std::string& content,
                       const nlohmann::json& metadata = {}) = 0;
    virtual std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) = 0;
    virtual std::vector<MemoryEntry> list(const std::string& filter = "") = 0;
    virtual void remove(const std::string& id) = 0;
};
```

| Method | Signature | Description |
|---|---|---|
| `write` | `(id, content, metadata)` | Upsert an entry by id |
| `search` | `(query, top_k) → vector<MemoryEntry>` | Semantic search; returns top-k results by relevance |
| `list` | `(filter) → vector<MemoryEntry>` | List all entries, optionally filtered |
| `remove` | `(id)` | Delete an entry |

## `MemoryEntry`

```cpp
struct MemoryEntry {
    std::string    id;
    std::string    content;
    nlohmann::json metadata;
    float          score{0.0f};   // relevance score from semantic search
};
```

`score` is populated by `search` implementations; `list` typically returns `score=0.0`.

## `NoOpMemoryBackend`

```cpp
class NoOpMemoryBackend : public MemoryBackend {
    void write(const std::string&, const std::string&, const nlohmann::json&) override {}
    std::vector<MemoryEntry> search(const std::string&, int) override { return {}; }
    std::vector<MemoryEntry> list(const std::string&) override { return {}; }
    void remove(const std::string&) override {}
};
```

Used automatically when `AgentManager` is constructed without a `memory` argument.

## Implementing a Real Backend

```cpp
class ChromaBackend : public MemoryBackend {
public:
    void write(const std::string& id, const std::string& content,
               const nlohmann::json& metadata) override {
        m_client.upsert(id, embed(content), metadata);
    }
    std::vector<MemoryEntry> search(const std::string& query, int top_k) override {
        auto hits = m_client.query(embed(query), top_k);
        // convert hits to MemoryEntry ...
    }
    // ...
};

auto mgr = AgentManager(config, llm, std::make_shared<ChromaBackend>());
```

## Real Implementation

The engine ships `AIModelMemoryBackend` which implements `MemoryBackend` via `AIModel::Embed` and `AIModel::Search`:

```cpp
auto model  = std::make_unique<AIModelVLLM>(...);
auto memory = std::make_shared<AIModelMemoryBackend>(*model);
auto mgr    = AgentManager(config, llm, memory);
```

See [`AIModelMemoryBackend`](ai_model_memory_backend.md) for full details.

## Related Components

- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — production `MemoryBackend` backed by `AIModel`
- [`MemoryWriteAction`](memory_actions.md), [`MemoryReadAction`](memory_actions.md), [`MemoryListAction`](memory_actions.md) — action wrappers
- [`AgentContext`](agent_context.md) — holds `shared_ptr<MemoryBackend>`; exposes `ctx.memory()`
- [`AgentManager`](agent_manager.md) — receives the `MemoryBackend` at construction
