#include "aimodel_llama.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AIModelLlama::AIModelLlama(const std::string& model_path,
                           int context_size, int thread_count,
                           size_t headroom_bytes)
    : m_context_size(context_size)
    , m_thread_count(thread_count)
{
    // ── 1. Read GGUF metadata (fast — weights not loaded) ─────────────────────
    GGUFModelInfo model_info;
    try {
        model_info = gguf_read_metadata(model_path);
    } catch (const std::exception& ex) {
        std::cerr << "[AIModelLlama] GGUF metadata read failed: " << ex.what()
                  << " — falling back to CPU-only mode\n";
    }

    // ── 2. Compute optimal VRAM plan ──────────────────────────────────────────
    size_t headroom = headroom_bytes > 0
        ? headroom_bytes
        : 512ULL * 1024 * 1024; // default 512 MiB

    if (model_info.n_layers > 0) {
        m_vram_plan = plan_vram(model_info,
                                static_cast<size_t>(context_size),
                                headroom);
        vram_plan_print(m_vram_plan, model_path);
    }

    // ── 3. Load model with computed n_gpu_layers ──────────────────────────────
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers       = 9999; // all layers to GPU; OOM if model doesn't fit

    m_model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!m_model)
        throw std::runtime_error(
            "AIModelLlama: failed to load model from \"" + model_path + "\"");

    // Grab model name
    char name_buf[256];
    llama_model_desc(m_model, name_buf, sizeof name_buf);
    m_model_name = name_buf;
    if (m_model_name.empty())
        m_model_name = std::filesystem::path(model_path).stem().string();

    // ── 4. Create inference context ───────────────────────────────────────────
    llama_context_params ctx_params   = llama_context_default_params();
    ctx_params.n_ctx                  = static_cast<uint32_t>(context_size);
    ctx_params.n_batch                = static_cast<uint32_t>(context_size);
    ctx_params.n_threads              = static_cast<uint32_t>(thread_count);
    ctx_params.n_threads_batch        = static_cast<uint32_t>(thread_count);

    m_inference_ctx = llama_init_from_model(m_model, ctx_params);
    if (!m_inference_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        throw std::runtime_error(
            "AIModelLlama: failed to create inference context for \"" + model_path + "\"");
    }

    // LayerStreamer is only needed when layers overflow to CPU.
    // With n_gpu_layers=9999 everything is on GPU, so no streaming required.
}

// ─────────────────────────────────────────────────────────────────────────────
// Destruction
// ─────────────────────────────────────────────────────────────────────────────

AIModelLlama::~AIModelLlama()
{
    m_streamer.reset(); // must happen before llama_backend_free
    if (m_inference_ctx) { llama_free(m_inference_ctx); m_inference_ctx = nullptr; }
    if (m_model)         { llama_model_free(m_model);   m_model         = nullptr; }
    llama_backend_free();
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel pure-virtual overrides
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModelLlama::GetModelName() const {
    return m_model_name;
}

int AIModelLlama::GetMaxContextLength() const {
    return m_context_size;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tokenize
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> AIModelLlama::Tokenize(const std::string& text, bool add_bos) const
{
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    int token_count = llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,
        add_bos, /*special=*/false);
    if (token_count < 0) token_count = -token_count;

    std::vector<llama_token> token_list(static_cast<std::size_t>(token_count));
    llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        token_list.data(), static_cast<int32_t>(token_list.size()),
        add_bos, /*special=*/false);
    return token_list;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawGenerate
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModelLlama::RawGenerate(const std::string& prompt, float t, int max)
{
    auto tokens = Tokenize(prompt, /*add_bos=*/true);

    llama_memory_clear(llama_get_memory(m_inference_ctx), /*data=*/true);

    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size()));
    if (llama_decode(m_inference_ctx, prompt_batch) != 0)
        throw std::runtime_error(
            "AIModelLlama::RawGenerate: llama_decode failed during prompt evaluation");

    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(t));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string generated;
    generated.reserve(static_cast<size_t>(max) * 4);

    const llama_vocab* vocab = llama_model_get_vocab(m_model);

    for (int i = 0; i < max; ++i) {
        llama_token new_id = llama_sampler_sample(sampler, m_inference_ctx, -1);

        if (llama_vocab_is_eog(vocab, new_id)) break;

        char piece[32];
        int piece_len = llama_token_to_piece(vocab, new_id, piece, sizeof piece, 0, false);
        if (piece_len > 0)
            generated.append(piece, static_cast<size_t>(piece_len));

        llama_batch next_batch = llama_batch_get_one(&new_id, 1);
        if (llama_decode(m_inference_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error(
                "AIModelLlama::RawGenerate: llama_decode failed during generation");
        }
    }

    llama_sampler_free(sampler);
    return generated;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawEmbed
// ─────────────────────────────────────────────────────────────────────────────

std::vector<float> AIModelLlama::RawEmbed(const std::string& text)
{
    llama_context_params ep  = llama_context_default_params();
    ep.n_ctx                 = 512;
    ep.n_threads             = static_cast<uint32_t>(m_thread_count);
    ep.embeddings            = true;
    ep.pooling_type          = LLAMA_POOLING_TYPE_MEAN;

    llama_context* ectx = llama_init_from_model(m_model, ep);
    if (!ectx)
        throw std::runtime_error("AIModelLlama::RawEmbed: failed to create embedding context");

    auto tokens = Tokenize(text, /*add_bos=*/false);
    llama_batch batch = llama_batch_get_one(
        tokens.data(), static_cast<int32_t>(tokens.size()));

    if (llama_decode(ectx, batch) != 0) {
        llama_free(ectx);
        throw std::runtime_error("AIModelLlama::RawEmbed: llama_decode failed");
    }

    int          dim     = llama_model_n_embd(m_model);
    const float* emb_ptr = llama_get_embeddings_seq(ectx, 0);
    if (!emb_ptr) {
        llama_free(ectx);
        throw std::runtime_error(
            "AIModelLlama::RawEmbed: no embedding output "
            "(does this model support embeddings?)");
    }

    std::vector<float> result(emb_ptr, emb_ptr + dim);
    llama_free(ectx);
    return result;
}
