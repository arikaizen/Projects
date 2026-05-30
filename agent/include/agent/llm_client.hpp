#pragma once
#include <functional>
#include <memory>
#include <string>
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
    struct Response {
        std::string content;
        bool        success{false};
        std::string error;
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
