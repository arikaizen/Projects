# LLMClient

`include/agent/llm_client.hpp`

## Overview

`LLMClient` is the abstract interface for calling a language model. Implementations may wrap a local model via `AIModel`, a remote API, or a deterministic mock for testing.

All four built-in stages call `ctx.llm().complete(request)`. `AgentContext` holds a `shared_ptr<LLMClient>` so one client is shared across all agents spawned by the same `AgentManager`.

## Interface

```cpp
class LLMClient {
public:
    struct Request {
        std::string system_prompt;
        std::string user_message;
        bool        json_mode{false};
        float       temperature{0.7f};
        int         max_tokens{2048};
    };

    struct Response {
        std::string content;
        bool        success{false};
        std::string error;
    };

    using Handler = std::function<Response(const Request&)>;

    virtual ~LLMClient() = default;
    virtual Response    complete(const Request& req) = 0;
    virtual std::string modelName() const = 0;
};
```

### `Request` Fields

| Field | Default | Meaning |
|---|---|---|
| `system_prompt` | — | System/instruction prompt (rendered by `PromptLoader`) |
| `user_message` | — | User turn — typically `"Produce your plan now."` or similar |
| `json_mode` | `false` | Hint to return valid JSON; used by `ReasonStage`, `InjectionStage`, `ValidateStage` |
| `temperature` | `0.7f` | Sampling temperature |
| `max_tokens` | `2048` | Maximum output tokens |

### `Response` Fields

| Field | Meaning |
|---|---|
| `content` | The model's completion string |
| `success` | `false` if the call failed |
| `error` | Error message when `success == false` |

### `Handler` Alias

```cpp
using Handler = std::function<Response(const Request&)>;
```

Defined on the base class for use by `MockLLMClient` and test helpers.

## `MockLLMClient`

```cpp
class MockLLMClient : public LLMClient {
public:
    explicit MockLLMClient(Handler h);
    Response    complete(const Request& req) override { return m_handler(req); }
    std::string modelName() const override { return "mock"; }
};
```

Delegates `complete` to the provided `std::function`. Used in all unit tests and as the default client in `am_create` when no LLM backend is configured:

```cpp
auto llm = std::make_shared<MockLLMClient>([](const LLMClient::Request&) {
    return LLMClient::Response{"[]", true, ""};
});
```

## Implementing a Real Client

```cpp
class MyClient : public LLMClient {
public:
    Response complete(const Request& req) override {
        auto text = m_convo.generate(req.system_prompt + "\n\n" + req.user_message,
                                     req.max_tokens, req.temperature);
        return {text, true, ""};
    }
    std::string modelName() const override { return "my-model"; }
private:
    MyConvo m_convo;
};
```

Pass it to `AgentManager`:

```cpp
auto manager = AgentManager(config, std::make_shared<MyClient>());
```

## Real Implementation

The engine ships `AIModelLLMClient` which implements `LLMClient` via `AIModel::Generate`:

```cpp
auto model = std::make_unique<AIModelVLLM>("http://localhost:8000", "llama-3.1-8b");
auto llm   = std::make_shared<AIModelLLMClient>(std::move(model));
auto mgr   = AgentManager(config, llm);
```

See [`AIModelLLMClient`](ai_model_llm_client.md) for full details.

## Related Components

- [`AIModelLLMClient`](ai_model_llm_client.md) — production `LLMClient` backed by `AIModel`
- [`AgentContext`](agent_context.md) — holds `shared_ptr<LLMClient>`; exposes `ctx.llm()`
- [`AgentManager`](agent_manager.md) — receives the `LLMClient` at construction
- [`Stages`](stages.md) — all four built-in stages call `ctx.llm().complete(...)`
