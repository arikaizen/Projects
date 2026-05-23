#include "aiconvo_vllm.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AIConvoVLLM::AIConvoVLLM(AIModelVLLM& model, const std::string& system_prompt)
    : AIConvo(model, system_prompt)
    , m_vllm_model(model)
{}

std::string AIConvoVLLM::GenerateReply(const std::vector<Message>& prompt_messages,
                                       float temperature, int max_tokens) {
    json messages = json::array();
    for (const auto& m : prompt_messages)
        messages.push_back({{"role", RoleToStr(m.role)}, {"content", m.content}});
    return m_vllm_model.ChatCompletion(messages, temperature, max_tokens);
}
