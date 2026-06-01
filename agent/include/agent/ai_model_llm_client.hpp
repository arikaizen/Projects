#pragma once
#include "agent/llm_client.hpp"
#include "ai_model/aimodel.hpp"
#include <memory>
#include <string>

namespace agent {

// Adapter: exposes any concrete AIModel (AIModelLlama, AIModelVLLM, or a custom
// subclass) to the agent engine through the LLMClient interface.
//
// Design notes:
//   - The agent engine owns the conversation state (AgentContext::history); each
//     Stage builds a fully-rendered system prompt every call.  We therefore use
//     the stateless AIModel::Generate layer, NOT AIConvo (whose own history would
//     duplicate the engine's).
//   - The engine's Request carries separate system_prompt + user_message but
//     AIModel::Generate takes a single prompt string, so we fold them together.
//   - json_mode appends an instruction asking for JSON-only output (AIModel has
//     no native JSON-mode switch).
//   - No exceptions escape complete(): AIModel throws on blank/empty/invalid
//     input, and we convert those into Response{success=false, error=...}.
//
// The adapter holds a reference to an externally-owned AIModel; the model must
// outlive the adapter (and therefore the AgentManager it is handed to).
class AIModelLLMClient : public LLMClient {
public:
    // Borrowing constructor: the caller owns the model and guarantees it
    // outlives the adapter.
    explicit AIModelLLMClient(AIModel& model) : m_model(model) {}

    // Owning constructor: the adapter takes ownership of the model, so it stays
    // alive as long as the LLMClient does (used when handing the client to an
    // AgentManager that will outlive the caller's scope).
    explicit AIModelLLMClient(std::unique_ptr<AIModel> model)
        : m_owned(std::move(model)), m_model(*m_owned) {}

    Response    complete(const Request& req) override;
    std::string modelName() const override { return m_model.GetModelName(); }

private:
    std::unique_ptr<AIModel> m_owned;   // non-null only for the owning ctor
    AIModel&                 m_model;
};

} // namespace agent
