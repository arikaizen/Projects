#include "aimodel_llama.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIModelLlama::AIModelLlama(const std::string& model_path,
                           int context_size, int thread_count)
    : m_context_size(context_size)
    , m_thread_count(thread_count)
{
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    m_model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!m_model)
        throw std::runtime_error("AIModelLlama: failed to load model from \"" + model_path + "\"");

    // Extract model name using llama_model_desc.
    char name_buf[256];
    llama_model_desc(m_model, name_buf, sizeof(name_buf));
    m_model_name = name_buf;
    if (m_model_name.empty()) {
        // Fall back to basename of model_path.
        m_model_name = std::filesystem::path(model_path).stem().string();
    }

    llama_context_params context_params  = llama_context_default_params();
    context_params.n_ctx                 = static_cast<uint32_t>(context_size);
    context_params.n_batch               = static_cast<uint32_t>(context_size);
    context_params.n_threads             = static_cast<uint32_t>(thread_count);
    context_params.n_threads_batch       = static_cast<uint32_t>(thread_count);

    m_inference_ctx = llama_init_from_model(m_model, context_params);
    if (!m_inference_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        throw std::runtime_error(
            "AIModelLlama: failed to create inference context for \"" + model_path + "\"");
    }
}

AIModelLlama::~AIModelLlama() {
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

std::vector<llama_token> AIModelLlama::Tokenize(const std::string& text, bool add_bos) const {
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    int token_count = llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,
        add_bos,
        /*special=*/false
    );
    if (token_count < 0) token_count = -token_count;

    std::vector<llama_token> token_list(static_cast<std::size_t>(token_count));
    llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        token_list.data(), static_cast<int32_t>(token_list.size()),
        add_bos,
        /*special=*/false
    );
    return token_list;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawGenerate (renamed from Decode)
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModelLlama::RawGenerate(const std::string& prompt, float t, int max) {
    auto tokens = Tokenize(prompt, /*add_bos=*/true);

    // Clear any KV state from a previous call — each generate is stateless.
    llama_memory_clear(llama_get_memory(m_inference_ctx), /*data=*/true);

    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );
    if (llama_decode(m_inference_ctx, prompt_batch) != 0)
        throw std::runtime_error("AIModelLlama::RawGenerate: llama_decode failed during prompt evaluation");

    llama_sampler_chain_params sampler_chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_chain_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(t));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string generated_text;
    generated_text.reserve(static_cast<std::size_t>(max) * 4);

    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    llama_token new_id;

    for (int i = 0; i < max; ++i) {
        new_id = llama_sampler_sample(sampler, m_inference_ctx, -1);

        if (llama_vocab_is_eog(vocab, new_id)) break;

        char piece[32];
        int piece_length = llama_token_to_piece(
            vocab, new_id, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            generated_text.append(piece, static_cast<std::size_t>(piece_length));

        llama_batch next_batch = llama_batch_get_one(&new_id, 1);
        if (llama_decode(m_inference_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("AIModelLlama::RawGenerate: llama_decode failed during generation");
        }
    }

    llama_sampler_free(sampler);
    return generated_text;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawEmbed
// ─────────────────────────────────────────────────────────────────────────────

std::vector<float> AIModelLlama::RawEmbed(const std::string& text) {
    llama_context_params embedding_params  = llama_context_default_params();
    embedding_params.n_ctx                 = 512;
    embedding_params.n_threads             = static_cast<uint32_t>(m_thread_count);
    embedding_params.embeddings            = true;
    embedding_params.pooling_type          = LLAMA_POOLING_TYPE_MEAN;

    llama_context* embedding_ctx = llama_init_from_model(m_model, embedding_params);
    if (!embedding_ctx)
        throw std::runtime_error("AIModelLlama::RawEmbed: failed to create embedding context");

    std::vector<llama_token> token_list = Tokenize(text, /*add_bos=*/false);

    llama_batch batch = llama_batch_get_one(
        token_list.data(), static_cast<int32_t>(token_list.size()));
    if (llama_decode(embedding_ctx, batch) != 0) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModelLlama::RawEmbed: llama_decode failed");
    }

    int          embedding_dimension = llama_model_n_embd(m_model);
    const float* embedding_data_ptr  = llama_get_embeddings_seq(embedding_ctx, 0);
    if (!embedding_data_ptr) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModelLlama::RawEmbed: no embedding output "
                                 "(does this model support embeddings?)");
    }

    std::vector<float> result(embedding_data_ptr, embedding_data_ptr + embedding_dimension);
    llama_free(embedding_ctx);
    return result;
}
