#pragma once
#include "aiconvo.hpp"
#include "aimodel_llama.hpp"
#include <llama.h>
#include <vector>

class AIConvoLlama final : public AIConvo {
public:
    AIConvoLlama(AIModelLlama& model,
                 const std::string& system_prompt = "You are a helpful assistant.");
    ~AIConvoLlama() override;

    void SaveState(const std::string& path);
    void LoadState(const std::string& path);
    void SetClearKvCacheEachTurn(bool clear) noexcept;
    bool GetClearKvCacheEachTurn() const noexcept;

protected:
    std::string GenerateReply(const std::vector<Message>& prompt_messages,
                              float temperature, int max_tokens) override;
    void OnChatFailure() noexcept override;

private:
    std::string FormatPrompt(const std::vector<Message>& messages) const;
    void        ClearKvCache() noexcept;

    AIModelLlama&            m_llama_model;
    llama_context*           m_conversation_ctx   = nullptr;
    int32_t                  m_tokens_processed   = 0;
    std::vector<llama_token> m_cached_prompt_tokens;
    bool                     m_clear_kv_each_turn = true;
};
