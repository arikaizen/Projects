#pragma once
#include "aimodel.hpp"
#include <llama.h>
#include <string>
#include <vector>

class AIModelLlama final : public AIModel {
public:
    AIModelLlama(const std::string& model_path,
                 int context_size = 4096, int thread_count = 4);
    ~AIModelLlama() override;

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    llama_model* ModelPtr()    const noexcept { return m_model; }
    int          CtxSize()     const noexcept { return m_context_size; }
    int          ThreadCount() const noexcept { return m_thread_count; }

    std::vector<llama_token> Tokenize(const std::string& text, bool add_bos) const;

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;

private:
    llama_model*   m_model         = nullptr;
    llama_context* m_inference_ctx = nullptr;
    int            m_context_size  = 0;
    int            m_thread_count  = 0;
    std::string    m_model_name;
};
