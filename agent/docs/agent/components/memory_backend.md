# MemoryBackend

`include/agent/memory_backend.hpp`

---

## Overview

`MemoryBackend` is the abstract interface for an agent's long-term memory. Implementations connect to a vector database, SQLite, or any other store. The no-op default (`NoOpMemoryBackend`) compiles cleanly and lets the rest of the system run without a configured memory store.

It is supplied to `AgentManager` at construction and exposed to actions via `AgentContext::memory()`.

---

## Interface

```cpp
class MemoryBackend {
public:
    virtual void write(const std::string& id, const std::string& content,
                       const nlohmann::json& metadata = {}) = 0;
    virtual std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) = 0;
    virtual std::vector<MemoryEntry> list(const std::string& filter = "") = 0;
    virtual void remove(const std::string& id) = 0;
};
```

### MemoryEntry

```cpp
struct MemoryEntry {
    std::string    id;
    std::string    content;
    nlohmann::json metadata;
    float          score{0.0f};   // relevance from search(); 0 from list()
};
```

---

## NoOpMemoryBackend

```cpp
class NoOpMemoryBackend : public MemoryBackend {
    void write(...) override {}
    std::vector<MemoryEntry> search(...) override { return {}; }
    std::vector<MemoryEntry> list(...)   override { return {}; }
    void remove(...)                     override {}
};
```

Used automatically when `AgentManager` is constructed without a `memory` argument (and by the default C ABI `am_create`).

---

## Real Implementation

[`AIModelMemoryBackend`](ai_model_memory_backend.md) implements this interface using an [`AIModel`](ai_model.md)'s `Embed`/`Search`, providing semantic memory. One model can back both the LLM client and memory.

```cpp
auto mem = std::make_shared<agent::AIModelMemoryBackend>(model);
AgentManager manager(config, llm, mem);
```

### Implementing your own

```cpp
class ChromaBackend : public MemoryBackend {
    void write(const std::string& id, const std::string& content,
               const nlohmann::json& meta) override { /* upsert(embed(content)) */ }
    std::vector<MemoryEntry> search(const std::string& q, int k) override { /* query */ }
    // list / remove ...
};
```

> Implementations must provide their own thread-safety — agents may call memory concurrently.

---

## Consumed By

The three [memory actions](memory_actions.md): `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction`.

---

## Related

- [LLMClient](llm_client.md) — the sibling abstract interface
- [AIModelMemoryBackend](ai_model_memory_backend.md) — semantic implementation
- [Memory actions](memory_actions.md) — plan-level access
- [AgentContext](agent_context.md) — exposes `memory()`
