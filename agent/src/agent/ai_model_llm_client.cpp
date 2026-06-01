#include "agent/ai_model_llm_client.hpp"
#include <stdexcept>

namespace agent {

LLMClient::Response AIModelLLMClient::complete(const Request& req) {
    try {
        // Fold the engine's (system_prompt, user_message) pair into the single
        // prompt string AIModel::Generate expects.  A backend that needs a chat
        // template should apply it inside its RawGenerate override.
        std::string prompt;
        if (!req.system_prompt.empty()) {
            prompt += req.system_prompt;
            prompt += "\n\n";
        }
        prompt += req.user_message;

        if (req.json_mode) {
            prompt += "\n\nRespond with valid JSON only. Do not include any prose "
                      "outside the JSON value.";
        }

        std::string out = m_model.Generate(prompt, req.temperature, req.max_tokens);
        return Response{ std::move(out), /*success=*/true, /*error=*/"" };
    } catch (const std::exception& e) {
        return Response{ /*content=*/"", /*success=*/false, /*error=*/e.what() };
    } catch (...) {
        return Response{ "", false, "AIModelLLMClient::complete: unknown error" };
    }
}

} // namespace agent
