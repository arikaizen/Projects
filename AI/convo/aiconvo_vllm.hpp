#pragma once
#include "aiconvo.hpp"
#include "aimodel_vllm.hpp"

class AIConvoVLLM final : public AIConvo {
public:
    AIConvoVLLM(AIModelVLLM& model,
                const std::string& system_prompt = "You are a helpful assistant.");
protected:
    std::string GenerateReply(const std::vector<Message>& prompt_messages,
                              float temperature, int max_tokens) override;
private:
    AIModelVLLM& m_vllm_model;
};
