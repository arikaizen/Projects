# LLMClient

`include/agent/llm_client.hpp`

> The memory interface has its own page: [MemoryBackend](memory_backend.md). This page covers `LLMClient` and `MockLLMClient`; the `MemoryBackend` section below is a summary that links out.

---

## LLMClient

### Overview

`LLMClient` is an abstract interface for calling a language model. Implementations may wrap a local LLM (e.g. the project's `AIConvo` / llama.cpp), a remote API, or a deterministic mock for tests.

Stages call `ctx.llm().complete(request)` directly. `AgentContext` holds a `shared_ptr<LLMClient>`, so one client can be shared across all agents spawned by the same `AgentManager`.

### Interface

```cpp
class LLMClient {
public:
    struct Request {
        std::string system_prompt;
        std::string user_message;
        bool        json_mode{false};   // hint to return well-formed JSON
        float       temperature{0.7f};
        int         max_tokens{2048};
    };

    struct Response {
        std::string content;
        bool        success{false};
        std::string error;
    };

    using Handler = std::function<Response(const Request&)>;

    virtual Response    complete(const Request& req) = 0;
    virtual std::string modelName() const = 0;
};
```

| `Request` field | Meaning |
|---|---|
| `system_prompt` | System / instruction prompt (rendered by `PromptLoader`) |
| `user_message` | User turn — typically the task or continuation cue |
| `json_mode` | Ask the model to return valid JSON (used by `ReasonStage`, `InjectionStage`, `ValidateStage`) |
| `temperature` | Sampling temperature |
| `max_tokens` | Maximum output tokens |

| `Response` field | Meaning |
|---|---|
| `content` | The model's completion string |
| `success` | `false` if the call failed |
| `error` | Error message when `success == false` |

### `Handler` Alias

```cpp
using Handler = std::function<Response(const Request&)>;
```

Defined on the base class so test helpers and `MockLLMClient` share the same type.

---

## MockLLMClient

```cpp
class MockLLMClient : public LLMClient {
public:
    explicit MockLLMClient(Handler h);
    Response    complete(const Request& req) override;
    std::string modelName() const override { return "mock"; }
};
```

Accepts a `std::function<Response(const Request&)>` and delegates `complete` to it. Used in all unit tests to return deterministic responses without a real model:

```cpp
auto llm = std::make_shared<MockLLMClient>([](const LLMClient::Request& req) {
    return LLMClient::Response{
        R"({"plan":[{"name":"BashAction","id":"b1","inputs":{"command":"echo hi"}}]})",
        true, ""
    };
});
```

---

## Implementing a Real Client

```cpp
class MyLlamaClient : public LLMClient {
public:
    Response complete(const Request& req) override {
        // call llama.cpp / AIConvo
        auto text = m_convo.generate(req.system_prompt + "\n" + req.user_message,
                                     req.max_tokens, req.temperature);
        return {text, true, ""};
    }
    std::string modelName() const override { return "llama-3.1-8b"; }
private:
    AIConvo m_convo;
};
```

Pass the implementation to `AgentManager`:

```cpp
auto manager = AgentManager(config, std::make_shared<MyLlamaClient>());
```

---

## MemoryBackend

### Overview

`MemoryBackend` is an abstract interface for long-term agent memory. Implementations connect to a vector database, SQLite, or any other store. The no-op default (`NoOpMemoryBackend`) compiles cleanly and lets all other functionality work without a configured memory store.

### Interface

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
    float          score{0.0f};   // relevance score from semantic search
};
```

### NoOpMemoryBackend

```cpp
class NoOpMemoryBackend : public MemoryBackend {
    void write(...) override {}
    std::vector<MemoryEntry> search(...) override { return {}; }
    std::vector<MemoryEntry> list(...)   override { return {}; }
    void remove(...)                     override {}
};
```

Used automatically when `AgentManager` is constructed without a `memory` argument.

### Implementing a Real Backend

```cpp
class ChromaBackend : public MemoryBackend {
public:
    void write(const std::string& id, const std::string& content,
               const nlohmann::json& metadata) override {
        m_client.upsert(id, embed(content), metadata);
    }
    std::vector<MemoryEntry> search(const std::string& query, int top_k) override {
        auto hits = m_client.query(embed(query), top_k);
        // ... convert to MemoryEntry ...
    }
    // ...
};
```

Pass the implementation to `AgentManager`:

```cpp
auto manager = AgentManager(config, llm, std::make_shared<ChromaBackend>());
```

---

## Consumed By

| Consumer | Usage |
|---|---|
| `AgentContext::llm()` | Called by all four built-in stages |
| `AgentContext::memory()` | Called by `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction` |

---

## Real Implementations

These interfaces are abstract. The engine ships a working implementation pair built on the [`AIModel`](ai_model.md) hierarchy:

- [`AIModelLLMClient`](ai_model_llm_client.md) — implements `LLMClient` via `AIModel::Generate`
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — implements `MemoryBackend` via `AIModel::Embed` / `Search`

A single `AIModel` (e.g. `AIModelVLLM`, `AIModelLlama`) can back both at once. Until one is wired in, the defaults are `MockLLMClient` and `NoOpMemoryBackend`.

## Related Components

- [`AIModel`](ai_model.md) — concrete model hierarchy behind the adapters
- [`AgentContext`](agent.md) — holds `shared_ptr<LLMClient>` and `shared_ptr<MemoryBackend>`
- [`AgentManager`](agent_manager.md) — receives both at construction; distributes to contexts
- [`Stages`](stages.md) — call `ctx.llm().complete(...)` on every reasoning step
- [`Actions`](actions.md) — `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction`
