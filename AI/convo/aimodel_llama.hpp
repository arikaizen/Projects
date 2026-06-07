#pragma once
#include "aimodel.hpp"
#include "gguf_planner.hpp"
#include <llama.h>
#include <memory>
#include <string>
#include <vector>

// Forward-declare to avoid pulling in managed_buft.hpp / ggml headers here.
struct ggml_backend_buffer_type;
using ggml_backend_buffer_type_t = struct ggml_backend_buffer_type *;
struct llama_model_tensor_buft_override;

class AIModelLlama final : public AIModel {
public:
    // context_size   — KV context window in tokens
    // thread_count   — CPU threads (used only for embedding; all inference is GPU)
    // headroom_bytes — VRAM to keep free (0 → default 512 MiB)
    AIModelLlama(const std::string& model_path,
                 int    context_size   = 4096,
                 int    thread_count   = 4,
                 size_t headroom_bytes = 0);
    ~AIModelLlama() override;

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

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

    VRAMPlan m_vram_plan;

    // Overflow layer streaming state (populated when model > VRAM)
    struct OverflowTensor {
        void*  data_ptr;  // managed-memory pointer from loaded tensor
        size_t size;
    };
    // Per overflow-layer: list of tensors to prefetch before computing that layer
    std::vector<std::vector<OverflowTensor>> m_overflow_tensors; // [relative_layer][tensor]

    void collect_overflow_tensors(const GGUFModelInfo& info);
    void prefetch_overflow_layers();
};
