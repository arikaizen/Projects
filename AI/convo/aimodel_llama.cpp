#include "aimodel_llama.hpp"
#include "managed_buft.hpp"

#include <ggml.h>           // ggml_tensor, ggml_nbytes

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cuda_runtime.h>   // cudaMemPrefetchAsync, cudaStream_t

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns true when the model can be fully loaded into free VRAM.
static bool model_fits_in_vram(const GGUFModelInfo& info, size_t headroom)
{
    size_t free_vram = vram_query_free();
    if (free_vram == 0) return false;
    return info.total_bytes + headroom <= free_vram;
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AIModelLlama::AIModelLlama(const std::string& model_path,
                           int context_size, int thread_count,
                           size_t headroom_bytes)
    : m_context_size(context_size)
    , m_thread_count(thread_count)
{
    // ── 1. Read GGUF metadata (fast — only header is read, no weights) ────────
    GGUFModelInfo model_info;
    try {
        model_info = gguf_read_metadata(model_path);
    } catch (const std::exception& ex) {
        std::cerr << "[AIModelLlama] GGUF metadata read failed: " << ex.what()
                  << "\n  Falling back to best-effort full-GPU load.\n";
    }

    const size_t headroom = headroom_bytes > 0
        ? headroom_bytes
        : 512ULL * 1024 * 1024; // 512 MiB default

    // ── 2. Decide load strategy ───────────────────────────────────────────────
    //
    //  PATH A — model fits entirely in VRAM:
    //    n_gpu_layers = 9999, no overrides.
    //    All weights in regular CUDA VRAM.  Fastest possible inference.
    //
    //  PATH B — model is larger than VRAM:
    //    n_gpu_layers = 9999 (still — all COMPUTE on GPU).
    //    First W layers: regular CUDA VRAM (determined by plan_vram).
    //    Layers W..N-1: CUDA Unified Memory (cudaMallocManaged).
    //    ggml copies each managed-memory weight tensor to a small VRAM scratch
    //    buffer before the CUDA kernel runs, then reuses that scratch for the
    //    next tensor.  Peak VRAM for weights ≈ 1 layer at a time.
    //    cudaMemPrefetchAsync pre-migrates the next layer to GPU while the
    //    current one is computing.

    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers       = 9999; // all compute on GPU, always

    // Override storage for overflow layers (if any).
    // We keep the strings + override array alive through the load call.
    std::vector<std::string>                    override_patterns;
    std::vector<llama_model_tensor_buft_override> overrides;

    if (model_info.n_layers > 0) {
        m_vram_plan = plan_vram(model_info,
                                static_cast<size_t>(context_size),
                                headroom);
        vram_plan_print(m_vram_plan, model_path);

        if (m_vram_plan.needs_streaming) {
            // PATH B: route overflow layers to managed memory.
            const int W = m_vram_plan.n_gpu_layers;
            const int N = m_vram_plan.n_total_layers;

            std::cerr << "[AIModelLlama] PATH B: layers 0.." << (W-1)
                      << " in VRAM, layers " << W << ".." << (N-1)
                      << " in CUDA Unified Memory\n";

            override_patterns.reserve(static_cast<size_t>(N - W));
            overrides.reserve(static_cast<size_t>(N - W + 1));

            ggml_backend_buffer_type_t mbuft = managed_buft_get();
            for (int l = W; l < N; ++l) {
                override_patterns.push_back("blk." + std::to_string(l) + ".*");
                overrides.push_back({override_patterns.back().c_str(), mbuft});
            }
            overrides.push_back({nullptr, nullptr}); // null terminator

            model_params.tensor_buft_overrides = overrides.data();
        } else {
            std::cerr << "[AIModelLlama] PATH A: full model fits in VRAM\n";
        }
    }

    // ── 3. Load model ─────────────────────────────────────────────────────────
    m_model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!m_model)
        throw std::runtime_error(
            "AIModelLlama: failed to load model from \"" + model_path + "\"");

    char name_buf[256];
    llama_model_desc(m_model, name_buf, sizeof name_buf);
    m_model_name = name_buf;
    if (m_model_name.empty())
        m_model_name = std::filesystem::path(model_path).stem().string();

    // ── 4. Collect managed-memory tensor pointers for prefetching ─────────────
    if (m_vram_plan.needs_streaming && model_info.n_layers > 0)
        collect_overflow_tensors(model_info);

    // ── 5. Pre-warm: issue prefetch for all overflow layers ───────────────────
    //    This asks the CUDA driver to start migrating overflow pages to GPU now,
    //    so they're resident before inference begins.
    if (!m_overflow_tensors.empty())
        prefetch_overflow_layers();

    // ── 6. Create inference context ───────────────────────────────────────────
    llama_context_params ctx_params  = llama_context_default_params();
    ctx_params.n_ctx                 = static_cast<uint32_t>(context_size);
    ctx_params.n_batch               = static_cast<uint32_t>(context_size);
    ctx_params.n_threads             = static_cast<uint32_t>(thread_count);
    ctx_params.n_threads_batch       = static_cast<uint32_t>(thread_count);

    m_inference_ctx = llama_init_from_model(m_model, ctx_params);
    if (!m_inference_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        throw std::runtime_error(
            "AIModelLlama: failed to create inference context for \""
            + model_path + "\"");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// collect_overflow_tensors
//   After loading, walk the GGUF tensor list and record the data pointer +
//   size for every tensor that belongs to an overflow layer.
//   llama_model_get_tensor() gives us the loaded (managed-memory) pointer.
// ─────────────────────────────────────────────────────────────────────────────
void AIModelLlama::collect_overflow_tensors(const GGUFModelInfo& info)
{
    const int W = m_vram_plan.n_gpu_layers;
    const int N = m_vram_plan.n_total_layers;
    const int n_overflow = N - W;
    if (n_overflow <= 0) return;

    m_overflow_tensors.resize(static_cast<size_t>(n_overflow));

    for (const auto& ti : info.tensors) {
        // Parse layer index from tensor name "blk.N.*"
        int layer = -1;
        if (ti.name.size() >= 5 && ti.name[0] == 'b' && ti.name[1] == 'l'
                                 && ti.name[2] == 'k' && ti.name[3] == '.') {
            size_t dot = ti.name.find('.', 4);
            if (dot != std::string::npos) {
                try { layer = std::stoi(ti.name.substr(4, dot - 4)); }
                catch (...) {}
            }
        }
        if (layer < W || layer >= N) continue;

        struct ggml_tensor* t = llama_model_get_tensor(m_model, ti.name.c_str());
        if (!t || !t->data) continue;

        int rel = layer - W;
        m_overflow_tensors[static_cast<size_t>(rel)].push_back(
            {t->data, ggml_nbytes(t)});
    }

    size_t total_tensors = 0;
    for (auto& tv : m_overflow_tensors) total_tensors += tv.size();
    std::cerr << "[AIModelLlama] tracked " << total_tensors
              << " overflow tensors across " << n_overflow << " layers\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// prefetch_overflow_layers
//   Issue cudaMemPrefetchAsync for all overflow layers in order.
//   The CUDA driver will fill the GPU with as many layers as fit and page out
//   the oldest when new ones are needed during inference.
// ─────────────────────────────────────────────────────────────────────────────
void AIModelLlama::prefetch_overflow_layers()
{
    for (const auto& layer_tensors : m_overflow_tensors) {
        for (const auto& ot : layer_tensors)
            managed_buft_prefetch(ot.data_ptr, ot.size, /*device=*/0);
    }
    // Wait for prefetches to finish before the first decode.
    cudaDeviceSynchronize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Destruction
// ─────────────────────────────────────────────────────────────────────────────

AIModelLlama::~AIModelLlama()
{
    m_overflow_tensors.clear();
    if (m_inference_ctx) { llama_free(m_inference_ctx); m_inference_ctx = nullptr; }
    if (m_model)         { llama_model_free(m_model);   m_model         = nullptr; }
    llama_backend_free();
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel overrides
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModelLlama::GetModelName() const { return m_model_name; }
int         AIModelLlama::GetMaxContextLength() const { return m_context_size; }

// ─────────────────────────────────────────────────────────────────────────────
// Tokenize
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> AIModelLlama::Tokenize(const std::string& text, bool add_bos) const
{
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    int n = llama_tokenize(vocab, text.c_str(),
                           static_cast<int32_t>(text.size()),
                           nullptr, 0, add_bos, false);
    if (n < 0) n = -n;
    std::vector<llama_token> toks(static_cast<size_t>(n));
    llama_tokenize(vocab, text.c_str(),
                   static_cast<int32_t>(text.size()),
                   toks.data(), static_cast<int32_t>(toks.size()),
                   add_bos, false);
    return toks;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawGenerate
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModelLlama::RawGenerate(const std::string& prompt, float t, int max)
{
    auto tokens = Tokenize(prompt, true);

    // Before each decode, prefetch the next cycle of overflow layers so CUDA
    // has them resident by the time ggml copies them to VRAM scratch.
    if (!m_overflow_tensors.empty())
        prefetch_overflow_layers();

    llama_memory_clear(llama_get_memory(m_inference_ctx), true);

    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size()));
    if (llama_decode(m_inference_ctx, prompt_batch) != 0)
        throw std::runtime_error(
            "AIModelLlama::RawGenerate: llama_decode failed on prompt");

    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(t));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string out;
    out.reserve(static_cast<size_t>(max) * 4);

    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    for (int i = 0; i < max; ++i) {
        llama_token tok = llama_sampler_sample(sampler, m_inference_ctx, -1);
        if (llama_vocab_is_eog(vocab, tok)) break;

        char piece[32];
        int  plen = llama_token_to_piece(vocab, tok, piece, sizeof piece, 0, false);
        if (plen > 0) out.append(piece, static_cast<size_t>(plen));

        llama_batch b = llama_batch_get_one(&tok, 1);
        if (llama_decode(m_inference_ctx, b) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error(
                "AIModelLlama::RawGenerate: llama_decode failed on token");
        }
    }

    llama_sampler_free(sampler);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawEmbed
// ─────────────────────────────────────────────────────────────────────────────

std::vector<float> AIModelLlama::RawEmbed(const std::string& text)
{
    llama_context_params ep = llama_context_default_params();
    ep.n_ctx          = 512;
    ep.n_threads      = static_cast<uint32_t>(m_thread_count);
    ep.embeddings     = true;
    ep.pooling_type   = LLAMA_POOLING_TYPE_MEAN;

    llama_context* ectx = llama_init_from_model(m_model, ep);
    if (!ectx)
        throw std::runtime_error("AIModelLlama::RawEmbed: failed to create context");

    auto tokens = Tokenize(text, false);
    llama_batch b = llama_batch_get_one(tokens.data(),
                                        static_cast<int32_t>(tokens.size()));
    if (llama_decode(ectx, b) != 0) {
        llama_free(ectx);
        throw std::runtime_error("AIModelLlama::RawEmbed: llama_decode failed");
    }

    int          dim = llama_model_n_embd(m_model);
    const float* emb = llama_get_embeddings_seq(ectx, 0);
    if (!emb) {
        llama_free(ectx);
        throw std::runtime_error(
            "AIModelLlama::RawEmbed: no embedding output "
            "(does this model support embeddings?)");
    }

    std::vector<float> result(emb, emb + dim);
    llama_free(ectx);
    return result;
}
