#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

// Abstract interface for calling an LLM. Implementations may wrap AIConvo,
// a remote API, or a deterministic mock for tests.
class LLMClient {
public:
    struct Request;
    struct Response;
    using Handler = std::function<Response(const Request&)>;

    struct Request {
        std::string system_prompt;
        std::string user_message;
        bool        json_mode{false};  // hint to return well-formed JSON
        float       temperature{0.7f};
        int         max_tokens{2048};
    };
    // A single native tool-call block returned by the model.
    struct ToolCall {
        std::string    id;          // model-assigned call id (may be empty)
        std::string    name;        // tool/function name
        nlohmann::json arguments;   // parsed argument object
    };

    struct Response {
        std::string           content;
        bool                  success{false};
        std::string           error;
        std::vector<ToolCall> tool_calls;  // non-empty when model issued tool calls
    };

    virtual ~LLMClient() = default;
    virtual Response    complete(const Request& req) = 0;
    virtual std::string modelName() const = 0;
};

// Deterministic mock for unit tests.
class MockLLMClient : public LLMClient {
public:
    using Handler = std::function<Response(const Request&)>;
    explicit MockLLMClient(Handler h) : m_handler(std::move(h)) {}

    Response    complete(const Request& req) override { return m_handler(req); }
    std::string modelName() const override { return "mock"; }

private:
    Handler m_handler;
};

} // namespace agent
