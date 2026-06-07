#pragma once
#include "aimodel.hpp"
#include "gguf_planner.hpp"
#include "layer_streamer.hpp"
#include <llama.h>
#include <memory>
#include <string>
#include <vector>

class AIModelLlama final : public AIModel {
public:
    // context_size  — KV context window (tokens)
    // thread_count  — CPU threads for non-GPU layers
    // headroom_bytes — VRAM to keep free; 0 → use default (512 MiB)
    AIModelLlama(const std::string& model_path,
                 int    context_size  = 4096,
                 int    thread_count  = 4,
                 size_t headroom_bytes = 0);
    ~AIModelLlama() override;

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    // Expose the active VRAM plan (populated after construction).
    const VRAMPlan& GetVRAMPlan() const noexcept { return m_vram_plan; }

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

    VRAMPlan                      m_vram_plan;
    std::unique_ptr<LayerStreamer> m_streamer;   // null when all layers fit in VRAM
};
