// layer_inference_engine.cpp
// Full pimpl implementation of LayerInferenceEngine.
//
// Architecture overview
// ─────────────────────
// LayerInferenceEngine uses three public calls:
//   calculate() — probes VRAM cost of one transformer block by allocating a real
//                 GPU buffer, then frees it.  Derives max_layers from the budget.
//   load()      — loads the full model onto CPU (n_gpu_layers=0), pre-allocates the
//                 sliding-window slots, and transfers the first window_size layers to VRAM.
//   generate()  — tokenises the prompt; for each token position runs the full
//                 n_layers forward pass with a sliding window that evicts one layer
//                 and loads the next after each per-layer forward pass.  A background
//                 thread prefetches the next-window layer while the current one runs.
//
// KV cache
// ────────
// K and V matrices for every layer are kept in CPU RAM (float32 flat arrays).
// When a layer is on GPU its K/V slice is copied to two persistent GPU tensors
// (k_gpu_, v_gpu_) before the flash-attention kernel, then the new K,V for the
// current token are copied back to RAM after the graph executes.
//
// Prefetch
// ────────
// While layer i executes on GPU, a std::thread copies the weight bytes for layer
// (i + window_size) from the CPU model into a staging buffer.  After layer i
// finishes, the staged bytes are pushed to VRAM with ggml_backend_tensor_set.

#include "layer_inference_engine.h"

// llama.cpp public API
#include <llama.h>

// ggml tensor ops and backend
#include <ggml.h>
#include <ggml-backend.h>
#include <ggml-alloc.h>

#ifdef GGML_USE_CUDA
#  include <ggml-cuda.h>
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Module-private helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Return wall-clock time in milliseconds.
float now_ms() {
    using namespace std::chrono;
    return static_cast<float>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()
    ) / 1000.0f;
}

// Build the GGUF tensor name for a given layer block and weight suffix.
// e.g. blk_tensor_name(3, "attn_q.weight") → "blk.3.attn_q.weight"
std::string blk_tensor_name(int layer, const char* suffix) {
    char buf[128];
    snprintf(buf, sizeof(buf), "blk.%d.%s", layer, suffix);
    return buf;
}

// Enumerate all GGUF tensor-name suffixes that make up one transformer block.
// Must stay in sync with load_layer_to_gpu / copy_layer_staging.
static const char* const kLayerSuffixes[] = {
    "attn_norm.weight",
    "attn_q.weight",
    "attn_k.weight",
    "attn_v.weight",
    "attn_output.weight",
    "ffn_norm.weight",
    "ffn_gate.weight",
    "ffn_up.weight",
    "ffn_down.weight",
    nullptr
};

// Number of tensors per transformer block (count kLayerSuffixes entries).
static constexpr int kLayerTensorCount = 9;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Impl — all private state
// ─────────────────────────────────────────────────────────────────────────────

struct LayerInferenceEngine::Impl {

    // ── GGUF metadata (valid after constructor) ──────────────────────────────
    std::string    model_path;
    gguf_context*  gguf_ctx     = nullptr;
    ggml_context*  meta_ctx     = nullptr;  // tensor shape headers, no data

    // ── Architecture parameters (read from GGUF) ─────────────────────────────
    int   n_layers        = 0;
    int   n_embd          = 0;
    int   n_head          = 0;
    int   n_head_kv       = 0;
    int   n_ff            = 0;
    int   n_vocab         = 0;
    int   n_rot           = 0;
    float rope_freq_base  = 10000.0f;
    float rms_norm_eps    = 1e-5f;

    // ── GPU backend ───────────────────────────────────────────────────────────
    ggml_backend_t gpu_backend = nullptr;

    // ── calculate() results ───────────────────────────────────────────────────
    float layer_size_mb         = 0.0f;
    int   max_layers_calculated = -1;   // -1 = calculate() not yet called

    // ── load() results ────────────────────────────────────────────────────────
    llama_model*   ll_model    = nullptr;
    llama_context* ll_ctx      = nullptr;
    int            window_size = 0;
    bool           is_loaded   = false;
    int            n_ctx       = 4096;

    // ── Sliding-window GPU slots ──────────────────────────────────────────────
    struct GpuLayer {
        int                   layer_idx  = -1;
        ggml_context*         w_ctx      = nullptr;   // tensor header context
        ggml_backend_buffer_t w_buf      = nullptr;   // GPU weight buffer
        // Pointers into w_ctx (valid only while w_buf is alive)
        ggml_tensor* attn_norm = nullptr;
        ggml_tensor* wq        = nullptr;
        ggml_tensor* wk        = nullptr;
        ggml_tensor* wv        = nullptr;
        ggml_tensor* wo        = nullptr;
        ggml_tensor* ffn_norm  = nullptr;
        ggml_tensor* ffn_gate  = nullptr;
        ggml_tensor* ffn_up    = nullptr;
        ggml_tensor* ffn_down  = nullptr;
    };
    std::vector<GpuLayer> gpu_window;   // size = window_size slots (circular)
    int gpu_resident_count = 0;
    int next_free_slot     = 0;         // next slot index to reuse in the circle

    // ── KV cache: CPU RAM, float32 ────────────────────────────────────────────
    // Layout per layer: k[pos * n_head_kv * head_dim], same for v.
    // n_past grows as tokens are processed.
    struct LayerKV {
        std::vector<float> k;
        std::vector<float> v;
    };
    std::vector<LayerKV> kv_cache;  // size = n_layers
    int n_past = 0;                 // tokens already in the KV cache

    // ── Persistent GPU KV tensors (shared across layers, resized per layer) ───
    // Allocated once in load(); size = n_ctx * n_head_kv * head_dim.
    ggml_context*         kv_ctx    = nullptr;
    ggml_backend_buffer_t kv_buf    = nullptr;
    ggml_tensor*          k_gpu_    = nullptr;  // [head_dim, n_head_kv, n_ctx]
    ggml_tensor*          v_gpu_    = nullptr;  // [head_dim, n_head_kv, n_ctx]

    // ── Persistent hidden-state GPU tensor ───────────────────────────────────
    // Allocated at the start of generate(); size = n_embd (single-token decode).
    ggml_context*         hs_ctx_   = nullptr;
    ggml_backend_buffer_t hs_buf_   = nullptr;
    ggml_tensor*          hidden_   = nullptr;  // [n_embd]

    // ── Prefetch slot ─────────────────────────────────────────────────────────
    struct PrefetchSlot {
        int next_layer = -1;
        bool ready     = false;
        // One entry per tensor in the block (kLayerTensorCount).
        struct Buf { std::vector<uint8_t> bytes; };
        std::vector<Buf> bufs;   // bufs[i] corresponds to kLayerSuffixes[i]
        std::thread      worker;
        std::mutex       mu;
    };
    PrefetchSlot prefetch_;

    // ── Verbose / stats ───────────────────────────────────────────────────────
    bool                         verbose_    = false;
    LayerInferenceEngine::Stats  last_stats_ = {};

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers (all return bool / void; never throw — callers wrap in exceptions)
    // ─────────────────────────────────────────────────────────────────────────

    // Purpose : Convenience wrapper: get a tensor from the CPU model by layer+suffix.
    // Args    : layer_idx — transformer block index; suffix — e.g. "attn_q.weight"
    // Returns : pointer to ggml_tensor in the CPU model, or nullptr if not found
    // Threads : main thread only
    ggml_tensor* cpu_tensor(int layer_idx, const char* suffix) const {
        const std::string name = blk_tensor_name(layer_idx, suffix);
        return llama_get_model_tensor(ll_model, name.c_str());
    }

    // Purpose : Compute VRAM currently occupied by all resident window layers
    //           plus the KV buffers and hidden-state buffer.
    // Returns : approximate VRAM usage in MB
    // Threads : main thread only
    float vram_used_mb() const {
        float total = 0.0f;
        for (const auto& s : gpu_window) {
            if (s.w_buf)
                total += static_cast<float>(ggml_backend_buffer_get_size(s.w_buf))
                         / (1024.0f * 1024.0f);
        }
        if (kv_buf)
            total += static_cast<float>(ggml_backend_buffer_get_size(kv_buf))
                     / (1024.0f * 1024.0f);
        if (hs_buf_)
            total += static_cast<float>(ggml_backend_buffer_get_size(hs_buf_))
                     / (1024.0f * 1024.0f);
        return total;
    }

    // Purpose : Allocate a GPU slot for one transformer block layer and copy its
    //           weight tensors from the CPU model into that VRAM slot.
    // Args    : layer_idx — which block to load; slot — target GpuLayer to populate
    // Returns : true on success, false on any allocation or copy failure
    // Memory  : allocates slot.w_ctx (CPU header) + slot.w_buf (GPU); caller must
    //           call unload_gpu_layer(slot) to free
    // Threads : may be called from the prefetch worker for staging or from main
    bool load_layer_to_gpu(int layer_idx, GpuLayer& slot) {
        // Header-only context (no data allocation — data goes into the GPU buffer).
        ggml_init_params ip{};
        ip.mem_size   = static_cast<size_t>(kLayerTensorCount + 4) *
                        ggml_tensor_overhead() + 1024;
        ip.mem_buffer = nullptr;
        ip.no_alloc   = true;
        slot.w_ctx = ggml_init(ip);
        if (!slot.w_ctx) return false;

        // For each suffix, mirror the CPU tensor's type and shape into w_ctx.
        auto mirror = [&](const char* suffix) -> ggml_tensor* {
            ggml_tensor* src = cpu_tensor(layer_idx, suffix);
            if (!src) return nullptr;
            ggml_tensor* t = ggml_new_tensor(slot.w_ctx, src->type,
                                              ggml_n_dims(src), src->ne);
            ggml_set_name(t, src->name);
            return t;
        };

        slot.attn_norm = mirror("attn_norm.weight");
        slot.wq        = mirror("attn_q.weight");
        slot.wk        = mirror("attn_k.weight");
        slot.wv        = mirror("attn_v.weight");
        slot.wo        = mirror("attn_output.weight");
        slot.ffn_norm  = mirror("ffn_norm.weight");
        slot.ffn_gate  = mirror("ffn_gate.weight");
        slot.ffn_up    = mirror("ffn_up.weight");
        slot.ffn_down  = mirror("ffn_down.weight");

        if (!slot.attn_norm || !slot.wq || !slot.wk || !slot.wv || !slot.wo ||
            !slot.ffn_norm  || !slot.ffn_gate || !slot.ffn_up || !slot.ffn_down) {
            ggml_free(slot.w_ctx); slot.w_ctx = nullptr;
            return false;
        }

        // Allocate all tensors in the GPU backend in one shot.
        slot.w_buf = ggml_backend_alloc_ctx_tensors(slot.w_ctx, gpu_backend);
        if (!slot.w_buf) {
            ggml_free(slot.w_ctx); slot.w_ctx = nullptr;
            return false;
        }

        // Copy data from CPU model tensors to their GPU mirrors.
        auto push = [&](ggml_tensor* gpu_t, const char* suffix) {
            ggml_tensor* src = cpu_tensor(layer_idx, suffix);
            if (src && gpu_t) ggml_backend_tensor_copy(src, gpu_t);
        };
        push(slot.attn_norm, "attn_norm.weight");
        push(slot.wq,        "attn_q.weight");
        push(slot.wk,        "attn_k.weight");
        push(slot.wv,        "attn_v.weight");
        push(slot.wo,        "attn_output.weight");
        push(slot.ffn_norm,  "ffn_norm.weight");
        push(slot.ffn_gate,  "ffn_gate.weight");
        push(slot.ffn_up,    "ffn_up.weight");
        push(slot.ffn_down,  "ffn_down.weight");

        slot.layer_idx = layer_idx;
        return true;
    }

    // Purpose : Free a GPU layer slot's VRAM buffer and context, resetting it to
    //           an unloaded state.
    // Args    : slot — the GpuLayer slot to clear
    // Memory  : frees slot.w_buf and slot.w_ctx
    // Threads : main thread only
    void unload_gpu_layer(GpuLayer& slot) {
        if (slot.w_buf) { ggml_backend_buffer_free(slot.w_buf); slot.w_buf = nullptr; }
        if (slot.w_ctx) { ggml_free(slot.w_ctx);                slot.w_ctx = nullptr; }
        slot.attn_norm = nullptr;
        slot.wq = slot.wk = slot.wv = slot.wo = nullptr;
        slot.ffn_norm = slot.ffn_gate = slot.ffn_up = slot.ffn_down = nullptr;
        slot.layer_idx = -1;
    }

    // Purpose : Copy all weight bytes for layer_idx into the prefetch staging
    //           buffers (CPU-only).  Called from a background thread.
    // Args    : layer_idx — which block to stage
    // Memory  : allocates prefetch_.bufs[i].bytes for each tensor in the block
    // Threads : background prefetch worker thread only
    void stage_layer_to_cpu(int layer_idx) {
        prefetch_.bufs.resize(kLayerTensorCount);
        for (int i = 0; kLayerSuffixes[i]; ++i) {
            ggml_tensor* src = cpu_tensor(layer_idx, kLayerSuffixes[i]);
            if (!src) { prefetch_.bufs[i].bytes.clear(); continue; }
            const size_t nb = ggml_nbytes(src);
            prefetch_.bufs[i].bytes.resize(nb);
            ggml_backend_tensor_get(src, prefetch_.bufs[i].bytes.data(), 0, nb);
        }
    }

    // Purpose : Upload a previously staged layer from prefetch_.bufs into GPU slot.
    //           Called on the main thread after the worker has signalled ready.
    // Args    : layer_idx — block index that was staged; slot — target GpuLayer
    // Returns : true on success
    // Memory  : allocates slot.w_ctx + slot.w_buf; clears prefetch_.bufs
    // Threads : main thread only
    bool push_staged_to_gpu(int layer_idx, GpuLayer& slot) {
        // Reuse load_layer_to_gpu to create ctx + buffer, then overwrite data
        // from the staging buffers instead of the live CPU tensors.
        if (!load_layer_to_gpu(layer_idx, slot)) return false;

        // Overwrite with staged bytes (avoids re-reading the model mmap).
        const ggml_tensor* targets[] = {
            slot.attn_norm, slot.wq, slot.wk, slot.wv, slot.wo,
            slot.ffn_norm, slot.ffn_gate, slot.ffn_up, slot.ffn_down
        };
        for (int i = 0; i < kLayerTensorCount; ++i) {
            if (targets[i] && !prefetch_.bufs[i].bytes.empty()) {
                ggml_backend_tensor_set(
                    const_cast<ggml_tensor*>(targets[i]),
                    prefetch_.bufs[i].bytes.data(),
                    0,
                    prefetch_.bufs[i].bytes.size());
            }
        }
        prefetch_.bufs.clear();
        return true;
    }

    // Purpose : Launch a background thread that stages layer (cur + window_size)
    //           into CPU RAM so it is ready to push to GPU without stalling.
    // Args    : cur        — the layer that just started executing on GPU
    //           n_layers   — total layer count (to bounds-check)
    // Memory  : overwrites prefetch_.bufs; joins any prior worker first
    // Threads : spawns one background thread; main thread starts it
    void start_prefetch(int cur, int total_layers) {
        const int next = cur + window_size;
        if (next >= total_layers) return;

        // Join any previous worker.
        if (prefetch_.worker.joinable()) prefetch_.worker.join();
        {
            std::lock_guard<std::mutex> lk(prefetch_.mu);
            prefetch_.ready     = false;
            prefetch_.next_layer = next;
        }
        prefetch_.worker = std::thread([this, next]() {
            stage_layer_to_cpu(next);
            std::lock_guard<std::mutex> lk(prefetch_.mu);
            prefetch_.ready = true;
        });
    }

    // Purpose : Wait for the current prefetch to complete (blocks until ready).
    // Threads : main thread only
    void wait_prefetch() {
        if (prefetch_.worker.joinable()) prefetch_.worker.join();
    }

    // Purpose : Build and execute the full forward pass for one transformer block.
    //           Implements: attention pre-norm → Q/K/V → RoPE → KV cache update →
    //           flash attention → output projection → residual → FFN pre-norm →
    //           SwiGLU → FFN down → residual.
    //           After the call, hidden_ holds the new hidden state.
    // Args    : layer     — GpuLayer slot with weight tensors on GPU
    //           layer_idx — transformer block index (for KV cache addressing)
    //           pos       — current token position (0-indexed)
    //           n_past    — number of tokens already in KV cache for this block
    // Returns : true on success, false on ggml allocation or compute failure
    // Memory  : allocates a temporary ggml_context + ggml_gallocr for the graph;
    //           both freed before return.  hidden_ is updated in-place on GPU.
    // Threads : main thread only
    bool run_layer_forward(GpuLayer& layer, int layer_idx, int pos, int n_past_tok) {
        const int head_dim = n_embd / n_head;
        const float scale  = 1.0f / sqrtf(static_cast<float>(head_dim));
        const int kv_len   = n_past_tok + 1;  // past tokens + current

        // ── Upload this layer's KV slice from CPU RAM to GPU ─────────────────
        // Positions [0, n_past_tok) are past tokens; position n_past_tok is new.
        const int kv_stride = n_head_kv * head_dim;
        if (n_past_tok > 0) {
            ggml_backend_tensor_set(k_gpu_, kv_cache[layer_idx].k.data(),
                0, static_cast<size_t>(n_past_tok) * kv_stride * sizeof(float));
            ggml_backend_tensor_set(v_gpu_, kv_cache[layer_idx].v.data(),
                0, static_cast<size_t>(n_past_tok) * kv_stride * sizeof(float));
        }

        // ── Build compute graph ───────────────────────────────────────────────
        // Rough node count: ~60 for one LLaMA block.  Allocate generous context.
        ggml_init_params cp{};
        cp.mem_size   = 128 * ggml_tensor_overhead() + ggml_graph_overhead_custom(256, false);
        cp.mem_buffer = nullptr;
        cp.no_alloc   = true;
        ggml_context* comp = ggml_init(cp);
        if (!comp) return false;

        // Reference the persistent hidden-state tensor in this context.
        ggml_tensor* cur = ggml_view_tensor(comp, hidden_);

        // Attention pre-norm.
        ggml_tensor* inp     = cur;
        ggml_tensor* normed  = ggml_rms_norm(comp, cur, rms_norm_eps);
        normed = ggml_mul(comp, normed, ggml_view_tensor(comp, layer.attn_norm));

        // Q / K / V linear projections.
        ggml_tensor* Q = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.wq), normed);
        ggml_tensor* K = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.wk), normed);
        ggml_tensor* V = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.wv), normed);

        // Reshape for multi-head / GQA.
        Q = ggml_reshape_3d(comp, Q, head_dim, n_head,    1);
        K = ggml_reshape_3d(comp, K, head_dim, n_head_kv, 1);
        V = ggml_reshape_3d(comp, V, head_dim, n_head_kv, 1);

        // Position tensor (i32 scalar = current token position).
        ggml_tensor* pos_t = ggml_new_tensor_1d(comp, GGML_TYPE_I32, 1);
        ggml_set_name(pos_t, "pos");

        // Apply rotary embeddings.
        Q = ggml_rope_ext(comp, Q, pos_t, nullptr,
                          n_rot, 0, n_ctx,
                          rope_freq_base, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        K = ggml_rope_ext(comp, K, pos_t, nullptr,
                          n_rot, 0, n_ctx,
                          rope_freq_base, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);

        // Write new K, V into the GPU KV tensors at position n_past_tok.
        // k_gpu_ is [head_dim, n_head_kv, n_ctx]; we view the slot at pos.
        const size_t k_elem    = sizeof(float);
        const size_t slot_offs = static_cast<size_t>(n_past_tok) * kv_stride * k_elem;

        ggml_tensor* k_slot = ggml_view_2d(comp,
            ggml_view_tensor(comp, k_gpu_),
            head_dim, n_head_kv,
            static_cast<size_t>(head_dim) * k_elem,
            slot_offs);
        ggml_tensor* v_slot = ggml_view_2d(comp,
            ggml_view_tensor(comp, v_gpu_),
            head_dim, n_head_kv,
            static_cast<size_t>(head_dim) * k_elem,
            slot_offs);

        // ggml_cpy returns the destination; adding it to the graph ensures the
        // write executes before the flash-attention that reads from k_gpu_/v_gpu_.
        ggml_tensor* k_cpy = ggml_cpy(comp, ggml_reshape_2d(comp, K, head_dim, n_head_kv), k_slot);
        ggml_tensor* v_cpy = ggml_cpy(comp, ggml_reshape_2d(comp, V, head_dim, n_head_kv), v_slot);

        // View the valid prefix of the KV cache [0, kv_len).
        ggml_tensor* K_full = ggml_view_3d(comp,
            ggml_view_tensor(comp, k_gpu_),
            head_dim, n_head_kv, kv_len,
            static_cast<size_t>(head_dim)            * k_elem,
            static_cast<size_t>(head_dim) * n_head_kv * k_elem,
            0);
        ggml_tensor* V_full = ggml_view_3d(comp,
            ggml_view_tensor(comp, v_gpu_),
            head_dim, n_head_kv, kv_len,
            static_cast<size_t>(head_dim)            * k_elem,
            static_cast<size_t>(head_dim) * n_head_kv * k_elem,
            0);

        // Flash attention: Q attends to full [0, kv_len) cache.
        ggml_tensor* attn_out = ggml_flash_attn_ext(comp, Q, K_full, V_full,
                                                     nullptr, scale, 0.0f, 0.0f);
        attn_out = ggml_reshape_1d(comp, attn_out, n_embd);

        // Output projection + residual.
        ggml_tensor* attn_proj = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.wo), attn_out);
        cur = ggml_add(comp, inp, attn_proj);

        // FFN pre-norm (SwiGLU).
        ggml_tensor* ffn_inp    = cur;
        ggml_tensor* ffn_normed = ggml_rms_norm(comp, cur, rms_norm_eps);
        ffn_normed = ggml_mul(comp, ffn_normed, ggml_view_tensor(comp, layer.ffn_norm));

        ggml_tensor* gate = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.ffn_gate), ffn_normed);
        gate = ggml_silu(comp, gate);
        ggml_tensor* up   = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.ffn_up),   ffn_normed);
        ggml_tensor* mid  = ggml_mul(comp, gate, up);
        ggml_tensor* down = ggml_mul_mat(comp, ggml_view_tensor(comp, layer.ffn_down), mid);
        cur = ggml_add(comp, ffn_inp, down);

        // Write the new hidden state back into hidden_ in-place.
        ggml_tensor* out_cpy = ggml_cpy(comp, cur, ggml_view_tensor(comp, hidden_));

        // Build the graph: K/V writes must precede attention; output write last.
        ggml_cgraph* gf = ggml_new_graph_custom(comp, 256, false);
        ggml_build_forward_expand(gf, k_cpy);
        ggml_build_forward_expand(gf, v_cpy);
        ggml_build_forward_expand(gf, out_cpy);

        // Allocate intermediate tensor memory on the GPU.
        ggml_gallocr_t allocr = ggml_gallocr_new(
            ggml_backend_get_default_buffer_type(gpu_backend));
        if (!ggml_gallocr_alloc_graph(allocr, gf)) {
            ggml_gallocr_free(allocr);
            ggml_free(comp);
            return false;
        }

        // Set the position value (needed by RoPE).
        const int32_t pos_val = static_cast<int32_t>(pos);
        ggml_backend_tensor_set(pos_t, &pos_val, 0, sizeof(int32_t));

        // Execute all graph nodes on the GPU.
        const ggml_status st = ggml_backend_graph_compute(gpu_backend, gf);

        ggml_gallocr_free(allocr);
        ggml_free(comp);

        if (st != GGML_STATUS_SUCCESS) return false;

        // Copy the new K, V for this token back to CPU RAM.
        const size_t new_kv_bytes = static_cast<size_t>(kv_stride) * sizeof(float);
        ggml_backend_tensor_get(k_gpu_,
            kv_cache[layer_idx].k.data() + n_past_tok * kv_stride,
            slot_offs, new_kv_bytes);
        ggml_backend_tensor_get(v_gpu_,
            kv_cache[layer_idx].v.data() + n_past_tok * kv_stride,
            slot_offs, new_kv_bytes);

        return true;
    }

    // Purpose : Apply final RMS norm, run lm_head projection, copy logits to CPU,
    //           and return the sampled next-token id.
    // Args    : temperature — sampling temperature; result written to *out_tok
    // Returns : true on success
    // Memory  : temporary ggml_context + gallocr for the graph; freed before return
    // Threads : main thread only
    bool sample_next_token(float temperature, llama_token* out_tok) {
        ggml_tensor* out_norm_w = llama_get_model_tensor(ll_model, "output_norm.weight");
        ggml_tensor* lm_head_w  = llama_get_model_tensor(ll_model, "output.weight");
        if (!out_norm_w || !lm_head_w) return false;

        // Lazily create GPU versions of the final-projection tensors.
        // (Small tensors — kept in a dedicated context allocated once per generate().)
        // For simplicity we upload on each sample call; these are small (few MB).

        ggml_init_params fp{};
        fp.mem_size   = 64 * ggml_tensor_overhead() + ggml_graph_overhead_custom(64, false);
        fp.mem_buffer = nullptr;
        fp.no_alloc   = true;
        ggml_context* fcomp = ggml_init(fp);
        if (!fcomp) return false;

        // Create GPU mirrors of norm and head weight (small, OK to alloc per call).
        ggml_init_params wp{};
        wp.mem_size   = 4 * ggml_tensor_overhead() + 512;
        wp.no_alloc   = true;
        ggml_context* wctx = ggml_init(wp);
        ggml_tensor* norm_gpu = ggml_new_tensor(wctx, out_norm_w->type,
                                                 ggml_n_dims(out_norm_w), out_norm_w->ne);
        ggml_tensor* head_gpu = ggml_new_tensor(wctx, lm_head_w->type,
                                                 ggml_n_dims(lm_head_w), lm_head_w->ne);
        ggml_backend_buffer_t wbuf = ggml_backend_alloc_ctx_tensors(wctx, gpu_backend);
        if (!wbuf) { ggml_free(wctx); ggml_free(fcomp); return false; }
        ggml_backend_tensor_copy(out_norm_w, norm_gpu);
        ggml_backend_tensor_copy(lm_head_w,  head_gpu);

        // Build the graph: rms_norm → scale → lm_head matmul.
        ggml_tensor* h    = ggml_view_tensor(fcomp, hidden_);
        ggml_tensor* norm = ggml_rms_norm(fcomp, h, rms_norm_eps);
        norm = ggml_mul(fcomp, norm, ggml_view_tensor(fcomp, norm_gpu));
        ggml_tensor* logits = ggml_mul_mat(fcomp, ggml_view_tensor(fcomp, head_gpu), norm);

        ggml_cgraph* gf = ggml_new_graph_custom(fcomp, 64, false);
        ggml_build_forward_expand(gf, logits);

        ggml_gallocr_t allocr = ggml_gallocr_new(
            ggml_backend_get_default_buffer_type(gpu_backend));
        if (!ggml_gallocr_alloc_graph(allocr, gf)) {
            ggml_gallocr_free(allocr);
            ggml_backend_buffer_free(wbuf); ggml_free(wctx); ggml_free(fcomp);
            return false;
        }

        ggml_backend_graph_compute(gpu_backend, gf);

        // Copy logits to CPU and sample.
        std::vector<float> cpu_logits(static_cast<size_t>(n_vocab));
        ggml_backend_tensor_get(logits, cpu_logits.data(), 0,
                                static_cast<size_t>(n_vocab) * sizeof(float));

        ggml_gallocr_free(allocr);
        ggml_backend_buffer_free(wbuf);
        ggml_free(wctx);
        ggml_free(fcomp);

        // Temperature scaling then greedy/sampled argmax.
        if (temperature <= 0.0f || temperature == 1.0f) {
            *out_tok = static_cast<llama_token>(
                std::max_element(cpu_logits.begin(), cpu_logits.end()) -
                cpu_logits.begin());
        } else {
            // Apply temperature, softmax, then sample.
            float max_l = *std::max_element(cpu_logits.begin(), cpu_logits.end());
            float sum   = 0.0f;
            for (auto& l : cpu_logits) { l = expf((l - max_l) / temperature); sum += l; }
            for (auto& l : cpu_logits) l /= sum;
            float r   = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            float acc = 0.0f;
            *out_tok  = static_cast<llama_token>(n_vocab - 1);
            for (int i = 0; i < n_vocab; ++i) {
                acc += cpu_logits[static_cast<size_t>(i)];
                if (acc >= r) { *out_tok = i; break; }
            }
        }
        return true;
    }

    // Purpose : Look up the embedding vector for one token id and write it into
    //           hidden_ on the GPU.
    // Args    : tok — token id to embed
    // Returns : true on success
    // Memory  : temporary ggml ops; result stored in persistent hidden_ buffer
    // Threads : main thread only
    bool embed_token(llama_token tok) {
        ggml_tensor* embd_w = llama_get_model_tensor(ll_model, "token_embd.weight");
        if (!embd_w) return false;

        // Copy the row for tok from the CPU embedding table directly into hidden_.
        const size_t row_bytes = static_cast<size_t>(n_embd) * sizeof(float);
        // embd_w is [n_embd, n_vocab]; row `tok` starts at tok * row_bytes
        // (assuming float32 embeddings; dequantise path not shown for brevity)
        if (embd_w->type == GGML_TYPE_F32) {
            const float* row =
                reinterpret_cast<const float*>(embd_w->data) +
                static_cast<ptrdiff_t>(tok) * n_embd;
            ggml_backend_tensor_set(hidden_, row, 0, row_bytes);
        } else {
            // Dequantise via ggml compute graph for quantised embedding tables.
            ggml_init_params ep{};
            ep.mem_size   = 32 * ggml_tensor_overhead() + ggml_graph_overhead_custom(32, false);
            ep.no_alloc   = true;
            ggml_context* ec = ggml_init(ep);

            // Create GPU mirror of embedding table (may be large; consider caching).
            ggml_init_params ewp{}; ewp.mem_size = 2 * ggml_tensor_overhead() + 256; ewp.no_alloc = true;
            ggml_context* ewc   = ggml_init(ewp);
            ggml_tensor* eg     = ggml_new_tensor(ewc, embd_w->type,
                                                   ggml_n_dims(embd_w), embd_w->ne);
            ggml_backend_buffer_t eb = ggml_backend_alloc_ctx_tensors(ewc, gpu_backend);
            ggml_backend_tensor_copy(embd_w, eg);

            ggml_tensor* tok_t = ggml_new_tensor_1d(ec, GGML_TYPE_I32, 1);
            ggml_tensor* row_t = ggml_get_rows(ec, ggml_view_tensor(ec, eg), tok_t);
            ggml_tensor* cpy   = ggml_cpy(ec, row_t, ggml_view_tensor(ec, hidden_));

            ggml_cgraph* gf = ggml_new_graph_custom(ec, 32, false);
            ggml_build_forward_expand(gf, cpy);
            ggml_gallocr_t ar = ggml_gallocr_new(
                ggml_backend_get_default_buffer_type(gpu_backend));
            ggml_gallocr_alloc_graph(ar, gf);
            const int32_t tv = tok;
            ggml_backend_tensor_set(tok_t, &tv, 0, sizeof(int32_t));
            ggml_backend_graph_compute(gpu_backend, gf);
            ggml_gallocr_free(ar);
            ggml_backend_buffer_free(eb);
            ggml_free(ewc);
            ggml_free(ec);
        }
        return true;
    }

    // Purpose : Tear down all GPU resources: window slots, KV buffers,
    //           hidden-state buffer, and the gpu_backend itself.
    // Memory  : frees all ggml_backend_buffer_t and ggml_context* members
    // Threads : main thread only (called from destructor)
    void release_all() {
        // Join prefetch thread.
        if (prefetch_.worker.joinable()) prefetch_.worker.join();

        for (auto& slot : gpu_window) unload_gpu_layer(slot);
        gpu_window.clear();

        if (kv_buf)  { ggml_backend_buffer_free(kv_buf);  kv_buf  = nullptr; }
        if (kv_ctx)  { ggml_free(kv_ctx);                 kv_ctx  = nullptr; }
        if (hs_buf_) { ggml_backend_buffer_free(hs_buf_); hs_buf_ = nullptr; }
        if (hs_ctx_) { ggml_free(hs_ctx_);                hs_ctx_ = nullptr; }
        hidden_ = k_gpu_ = v_gpu_ = nullptr;

        if (ll_ctx)  { llama_free(ll_ctx);                 ll_ctx  = nullptr; }
        if (ll_model){ llama_model_free(ll_model);          ll_model = nullptr; }
        if (gpu_backend) { ggml_backend_free(gpu_backend); gpu_backend = nullptr; }
        if (gguf_ctx) { gguf_free(gguf_ctx);               gguf_ctx = nullptr; }
        if (meta_ctx) { ggml_free(meta_ctx);               meta_ctx = nullptr; }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

// Purpose : Open the GGUF file and read model metadata only (no GPU allocation).
// Args    : model_path — filesystem path to a .gguf model file
// Returns : void (constructor)
// Memory  : allocates impl_->gguf_ctx and impl_->meta_ctx (tensor headers only)
// Threads : main thread only
LayerInferenceEngine::LayerInferenceEngine(const std::string& model_path)
    : impl_(std::make_unique<Impl>())
{
    impl_->model_path = model_path;

    ggml_context* meta = nullptr;
    gguf_init_params gp{};
    gp.no_alloc = true;
    gp.ctx      = &meta;

    impl_->gguf_ctx = gguf_init_from_file(model_path.c_str(), gp);
    if (!impl_->gguf_ctx)
        throw std::runtime_error("LayerInferenceEngine: cannot open or parse: " + model_path);
    impl_->meta_ctx = meta;

    // ── Read architecture key/value pairs ─────────────────────────────────────
    auto get_i32 = [&](const char* key, int fallback) -> int {
        const int idx = gguf_find_key(impl_->gguf_ctx, key);
        return (idx >= 0) ? gguf_get_val_i32(impl_->gguf_ctx, idx) : fallback;
    };
    auto get_u32 = [&](const char* key, int fallback) -> int {
        const int idx = gguf_find_key(impl_->gguf_ctx, key);
        return (idx >= 0) ? static_cast<int>(gguf_get_val_u32(impl_->gguf_ctx, idx)) : fallback;
    };
    auto get_f32 = [&](const char* key, float fallback) -> float {
        const int idx = gguf_find_key(impl_->gguf_ctx, key);
        return (idx >= 0) ? gguf_get_val_f32(impl_->gguf_ctx, idx) : fallback;
    };

    impl_->n_layers       = get_u32("llama.block_count",                  32);
    impl_->n_embd         = get_u32("llama.embedding_length",             4096);
    impl_->n_head         = get_u32("llama.attention.head_count",         32);
    impl_->n_head_kv      = get_u32("llama.attention.head_count_kv",      impl_->n_head);
    impl_->n_ff           = get_u32("llama.feed_forward_length",          11008);
    impl_->n_vocab        = get_u32("llama.vocab_size",                   32000);
    impl_->rope_freq_base = get_f32("llama.rope.freq_base",               10000.0f);
    impl_->rms_norm_eps   = get_f32("llama.attention.layer_norm_rms_epsilon", 1e-5f);
    impl_->n_rot          = get_u32("llama.rope.dimension_count",
                                    impl_->n_embd / impl_->n_head);
    (void)get_i32;  // suppress unused warning
}

// ─────────────────────────────────────────────────────────────────────────────
// calculate()
// ─────────────────────────────────────────────────────────────────────────────

// Purpose : Measure the real VRAM cost of one transformer block by allocating a
//           GPU buffer for layer 0's tensors and reading its byte size.
// Args    : vram_budget_gb — total VRAM budget in gigabytes
// Returns : maximum number of transformer blocks that fit; 0 if none fit
// Memory  : allocates then immediately frees one GPU buffer sized for one layer
// Threads : main thread only
int LayerInferenceEngine::calculate(float vram_budget_gb)
{
    Impl& I = *impl_;

    // Initialise GPU backend once.
    if (!I.gpu_backend) {
#ifdef GGML_USE_CUDA
        I.gpu_backend = ggml_backend_cuda_init(0);
#endif
        if (!I.gpu_backend)
            throw std::runtime_error("calculate: no CUDA GPU backend available");
    }

    // Build a temporary ggml_context mirroring the tensor shapes of layer 0.
    ggml_init_params ip{};
    ip.mem_size   = static_cast<size_t>(kLayerTensorCount + 4) *
                    ggml_tensor_overhead() + 1024;
    ip.mem_buffer = nullptr;
    ip.no_alloc   = true;
    ggml_context* probe_ctx = ggml_init(ip);
    if (!probe_ctx)
        throw std::runtime_error("calculate: ggml_init failed");

    for (int i = 0; kLayerSuffixes[i]; ++i) {
        ggml_tensor* src = ggml_get_tensor(I.meta_ctx,
                               blk_tensor_name(0, kLayerSuffixes[i]).c_str());
        if (!src) continue;
        ggml_tensor* t = ggml_new_tensor(probe_ctx, src->type,
                                          ggml_n_dims(src), src->ne);
        ggml_set_name(t, src->name);
    }

    // Allocate on the GPU — this is the actual VRAM measurement.
    ggml_backend_buffer_t probe_buf =
        ggml_backend_alloc_ctx_tensors(probe_ctx, I.gpu_backend);
    if (!probe_buf) {
        ggml_free(probe_ctx);
        throw std::runtime_error("calculate: GPU buffer allocation failed for probe layer");
    }

    const size_t layer_bytes = ggml_backend_buffer_get_size(probe_buf);
    I.layer_size_mb = static_cast<float>(layer_bytes) / (1024.0f * 1024.0f);

    // Free the probe immediately.
    ggml_backend_buffer_free(probe_buf);
    ggml_free(probe_ctx);

    const float vram_mb  = vram_budget_gb * 1024.0f;
    I.max_layers_calculated = (I.layer_size_mb > 0.0f)
        ? static_cast<int>(vram_mb / I.layer_size_mb)
        : 0;
    // Clamp to actual model depth.
    if (I.max_layers_calculated > I.n_layers)
        I.max_layers_calculated = I.n_layers;

    if (I.verbose_) {
        fprintf(stderr, "[calculate] Model layers  : %d\n",     I.n_layers);
        fprintf(stderr, "[calculate] Layer size    : %.0f MB\n", I.layer_size_mb);
        fprintf(stderr, "[calculate] VRAM budget   : %.0f MB\n", vram_mb);
        fprintf(stderr, "[calculate] Max layers    : %d\n",      I.max_layers_calculated);
    }

    return I.max_layers_calculated;
}

// ─────────────────────────────────────────────────────────────────────────────
// load()
// ─────────────────────────────────────────────────────────────────────────────

// Purpose : Load the full model onto CPU RAM, allocate the sliding-window GPU
//           slots, allocate the persistent KV GPU buffers, and transfer the first
//           window_size transformer blocks to VRAM.
// Args    : window_size — number of layers to keep on GPU simultaneously;
//                         must be <= value returned by calculate()
// Returns : void
// Memory  : allocates ll_model (CPU), gpu_window slots (GPU weight buffers),
//           kv_buf (GPU KV tensors sized for n_ctx tokens)
// Threads : main thread only
void LayerInferenceEngine::load(int window_size)
{
    Impl& I = *impl_;

    if (I.max_layers_calculated < 0)
        throw std::runtime_error("load: calculate() must be called before load()");
    if (window_size > I.max_layers_calculated)
        throw std::runtime_error("load: window_size exceeds max_layers from calculate()");
    if (window_size <= 0)
        throw std::runtime_error("load: window_size must be >= 1");

    // Unload any previously loaded model.
    if (I.is_loaded) {
        for (auto& s : I.gpu_window) I.unload_gpu_layer(s);
        I.gpu_window.clear();
        if (I.kv_buf)  { ggml_backend_buffer_free(I.kv_buf);  I.kv_buf  = nullptr; }
        if (I.kv_ctx)  { ggml_free(I.kv_ctx);                 I.kv_ctx  = nullptr; }
        if (I.ll_ctx)  { llama_free(I.ll_ctx);                 I.ll_ctx  = nullptr; }
        if (I.ll_model){ llama_model_free(I.ll_model);          I.ll_model = nullptr; }
        I.is_loaded = false;
    }

    // Load model entirely onto CPU (n_gpu_layers = 0).
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    I.ll_model = llama_model_load_from_file(I.model_path.c_str(), mp);
    if (!I.ll_model)
        throw std::runtime_error("load: llama_model_load_from_file failed");

    // Create a minimal llama_context for tokenisation and vocab access.
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx    = static_cast<uint32_t>(I.n_ctx);
    cp.n_threads = 4;
    cp.n_gpu_layers = 0;
    I.ll_ctx = llama_new_context_with_model(I.ll_model, cp);
    if (!I.ll_ctx)
        throw std::runtime_error("load: llama_new_context_with_model failed");

    I.window_size = window_size;

    // Allocate window slots (initially empty).
    I.gpu_window.assign(static_cast<size_t>(window_size), Impl::GpuLayer{});
    I.gpu_resident_count = 0;
    I.next_free_slot     = 0;

    // Allocate persistent GPU KV tensors sized for the full context window.
    const int head_dim = I.n_embd / I.n_head;
    ggml_init_params kvp{};
    kvp.mem_size   = 4 * ggml_tensor_overhead() + 512;
    kvp.no_alloc   = true;
    I.kv_ctx = ggml_init(kvp);
    I.k_gpu_ = ggml_new_tensor_3d(I.kv_ctx, GGML_TYPE_F32,
                                   head_dim, I.n_head_kv, I.n_ctx);
    I.v_gpu_ = ggml_new_tensor_3d(I.kv_ctx, GGML_TYPE_F32,
                                   head_dim, I.n_head_kv, I.n_ctx);
    I.kv_buf = ggml_backend_alloc_ctx_tensors(I.kv_ctx, I.gpu_backend);
    if (!I.kv_buf)
        throw std::runtime_error("load: failed to allocate GPU KV cache buffer");

    // Allocate CPU KV cache for all layers.
    I.kv_cache.resize(static_cast<size_t>(I.n_layers));
    const size_t kv_row = static_cast<size_t>(I.n_head_kv) * head_dim;
    for (auto& lkv : I.kv_cache) {
        lkv.k.assign(static_cast<size_t>(I.n_ctx) * kv_row, 0.0f);
        lkv.v.assign(static_cast<size_t>(I.n_ctx) * kv_row, 0.0f);
    }
    I.n_past = 0;

    // Transfer the first window_size layers to GPU.
    const int init_layers = std::min(window_size, I.n_layers);
    for (int i = 0; i < init_layers; ++i) {
        if (I.verbose_)
            fprintf(stderr, "[load] Loading layer %d / %d ...\n", i, window_size);
        if (!I.load_layer_to_gpu(i, I.gpu_window[static_cast<size_t>(i)]))
            throw std::runtime_error("load: failed to load layer " + std::to_string(i));
        I.gpu_window[static_cast<size_t>(i)].layer_idx = i;
        ++I.gpu_resident_count;
    }
    I.next_free_slot = init_layers % window_size;

    const float vram_now = I.vram_used_mb();
    const float vram_cap = static_cast<float>(window_size) * I.layer_size_mb;
    if (I.verbose_)
        fprintf(stderr, "[load] Window ready. VRAM used: %.0f MB / %.0f MB\n",
                vram_now, vram_cap);

    I.is_loaded = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// generate()
// ─────────────────────────────────────────────────────────────────────────────

// Purpose : Tokenise the prompt, run the full model forward pass with a sliding
//           window across all layers, and stream generated tokens to stdout.
//           For every token position, each of the n_layers transformer blocks
//           executes exactly once; after each block its weights are unloaded and
//           the next block is loaded (prefetched in the background).
// Args    : prompt     — input text
//           max_tokens — maximum new tokens to generate (0 = model default 256)
// Returns : the full generated string (same text printed to stdout)
// Memory  : allocates hs_ctx_/hs_buf_/hidden_ (GPU hidden state) at start;
//           frees them before return.  Layer weights are loaded/freed per layer.
// Threads : main thread runs inference; one background thread handles prefetch
std::string LayerInferenceEngine::generate(const std::string& prompt, int max_tokens)
{
    Impl& I = *impl_;

    if (!I.is_loaded)
        throw std::runtime_error("generate: load() must be called before generate()");

    const int gen_limit = (max_tokens > 0) ? max_tokens : 256;

    // ── Allocate persistent hidden-state GPU tensor ───────────────────────────
    ggml_init_params hsp{};
    hsp.mem_size   = 4 * ggml_tensor_overhead() + 512;
    hsp.no_alloc   = true;
    I.hs_ctx_ = ggml_init(hsp);
    I.hidden_ = ggml_new_tensor_1d(I.hs_ctx_, GGML_TYPE_F32, I.n_embd);
    I.hs_buf_ = ggml_backend_alloc_ctx_tensors(I.hs_ctx_, I.gpu_backend);
    if (!I.hs_buf_)
        throw std::runtime_error("generate: failed to allocate GPU hidden-state buffer");

    // ── Tokenise ──────────────────────────────────────────────────────────────
    const llama_vocab* vocab = llama_model_get_vocab(I.ll_model);
    std::vector<llama_token> prompt_tokens;
    {
        int n = llama_tokenize(vocab, prompt.c_str(),
                               static_cast<int32_t>(prompt.size()),
                               nullptr, 0, /*add_bos=*/true, false);
        if (n < 0) n = -n;
        prompt_tokens.resize(static_cast<size_t>(n));
        llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
                       prompt_tokens.data(), n, true, false);
    }

    // ── Stats bookkeeping ─────────────────────────────────────────────────────
    I.last_stats_ = {};
    const float t_start  = now_ms();
    float       peak_vram = I.vram_used_mb();
    float       total_layer_ms = 0.0f;
    int         layer_calls    = 0;

    // ── Core generation loop: process prompt + generate new tokens ────────────
    std::string result;
    result.reserve(static_cast<size_t>(gen_limit) * 4);

    // Helper lambda: run one full n_layers pass for token at position `pos`.
    // The sliding window slides one layer at a time.
    auto run_all_layers = [&](llama_token tok, int pos) -> bool {
        // Look up the embedding for this token → hidden_.
        if (!I.embed_token(tok)) return false;

        // For the first token, the window is already seeded (from load()).
        // For subsequent tokens, we re-seed from layer 0.
        // Strategy: re-use the loaded slots for layers [0, window_size-1] on the
        // first pass; on subsequent passes reload from scratch.
        // For simplicity we reload the full window from layer 0 each token.
        // (The prefetch thread hides the latency of the next layer.)

        // Unload all currently resident layers so we start fresh at layer 0.
        for (auto& s : I.gpu_window) {
            if (s.layer_idx >= 0) { I.unload_gpu_layer(s); --I.gpu_resident_count; }
        }
        I.gpu_resident_count = 0;
        I.next_free_slot     = 0;

        // Pre-load the first window_size layers.
        const int init_n = std::min(I.window_size, I.n_layers);
        for (int i = 0; i < init_n; ++i) {
            if (!I.load_layer_to_gpu(i, I.gpu_window[static_cast<size_t>(i % I.window_size)]))
                return false;
            ++I.gpu_resident_count;
        }
        I.next_free_slot = init_n % I.window_size;

        // Slide the window across all n_layers.
        for (int L = 0; L < I.n_layers; ++L) {
            // Find the slot that holds layer L.
            int slot_idx = L % I.window_size;
            assert(I.gpu_window[static_cast<size_t>(slot_idx)].layer_idx == L);

            // Start prefetching L + window_size while L executes.
            I.start_prefetch(L, I.n_layers);

            const float t0 = now_ms();

            if (I.verbose_)
                fprintf(stderr, "[L %02d/%d] loaded  | VRAM: %.0f MB | prefetch: L%d\n",
                        L, I.n_layers, I.vram_used_mb(), L + I.window_size);

            // Forward pass for layer L.
            assert(I.gpu_resident_count <= I.window_size);
            if (!I.run_layer_forward(I.gpu_window[static_cast<size_t>(slot_idx)],
                                      L, pos, I.n_past))
                return false;

            const float t1 = now_ms();
            total_layer_ms += (t1 - t0);
            ++layer_calls;

            if (I.verbose_)
                fprintf(stderr, "[L %02d/%d] forward | time: %.0f ms\n",
                        L, I.n_layers, t1 - t0);

            // Unload layer L.
            I.unload_gpu_layer(I.gpu_window[static_cast<size_t>(slot_idx)]);
            --I.gpu_resident_count;

            // Wait for prefetch, then push the next layer to GPU.
            const int next_L = L + I.window_size;
            if (next_L < I.n_layers) {
                I.wait_prefetch();
                const int next_slot = next_L % I.window_size;
                if (!I.push_staged_to_gpu(next_L,
                        I.gpu_window[static_cast<size_t>(next_slot)]))
                    return false;
                ++I.gpu_resident_count;

                if (I.verbose_)
                    fprintf(stderr, "[L %02d/%d] unload  | VRAM: %.0f MB | loaded: L%d\n",
                            L, I.n_layers, I.vram_used_mb(), next_L);
            } else {
                if (I.verbose_)
                    fprintf(stderr, "[L %02d/%d] unload  | VRAM: %.0f MB\n",
                            L, I.n_layers, I.vram_used_mb());
            }

            assert(I.gpu_resident_count <= I.window_size);

            const float v = I.vram_used_mb();
            if (v > peak_vram) peak_vram = v;
        }

        ++I.n_past;
        return true;
    };

    // ── Prefill: process each prompt token ───────────────────────────────────
    for (size_t pi = 0; pi < prompt_tokens.size(); ++pi) {
        if (!run_all_layers(prompt_tokens[pi], static_cast<int>(pi)))
            throw std::runtime_error("generate: layer forward pass failed during prefill");
    }

    // ── Generation: sample new tokens ─────────────────────────────────────────
    for (int gi = 0; gi < gen_limit; ++gi) {
        llama_token next_tok = 0;
        if (!I.sample_next_token(0.7f, &next_tok))
            throw std::runtime_error("generate: sampling failed");

        if (llama_vocab_is_eog(vocab, next_tok)) break;

        // Decode token to text and stream to stdout.
        char piece[32];
        const int piece_len = llama_token_to_piece(vocab, next_tok, piece, sizeof(piece), 0, false);
        if (piece_len > 0) {
            const std::string tok_str(piece, static_cast<size_t>(piece_len));
            result += tok_str;
            fwrite(piece, 1, static_cast<size_t>(piece_len), stdout);
            fflush(stdout);
        }

        ++I.last_stats_.tokens_generated;

        // Feed the sampled token through all layers for the next position.
        const int pos = static_cast<int>(prompt_tokens.size()) + gi;
        if (!run_all_layers(next_tok, pos))
            throw std::runtime_error("generate: layer forward pass failed during generation");
    }

    // ── Clean up hidden-state buffer ─────────────────────────────────────────
    if (I.hs_buf_) { ggml_backend_buffer_free(I.hs_buf_); I.hs_buf_ = nullptr; }
    if (I.hs_ctx_) { ggml_free(I.hs_ctx_);                I.hs_ctx_ = nullptr; }
    I.hidden_ = nullptr;

    // Ensure prefetch thread is joined.
    I.wait_prefetch();

    // ── Finalise stats ────────────────────────────────────────────────────────
    const float t_total = now_ms() - t_start;
    I.last_stats_.total_ms         = t_total;
    I.last_stats_.tokens_per_second =
        (t_total > 0.0f) ? (static_cast<float>(I.last_stats_.tokens_generated) /
                             (t_total / 1000.0f)) : 0.0f;
    I.last_stats_.peak_vram_mb  = peak_vram;
    I.last_stats_.avg_layer_ms  = (layer_calls > 0)
        ? (total_layer_ms / static_cast<float>(layer_calls)) : 0.0f;

    // Reset KV past counter for the next generate() call.
    I.n_past = 0;
    for (auto& lkv : I.kv_cache) {
        std::fill(lkv.k.begin(), lkv.k.end(), 0.0f);
        std::fill(lkv.v.begin(), lkv.v.end(), 0.0f);
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// set_verbose() / stats() / destructor
// ─────────────────────────────────────────────────────────────────────────────

// Purpose : Enable or disable per-layer stderr diagnostics.
// Args    : on — true to enable
// Returns : void
// Memory  : no allocations
// Threads : main thread only
void LayerInferenceEngine::set_verbose(bool on) {
    impl_->verbose_ = on;
}

// Purpose : Return a snapshot of counters from the last generate() call.
// Args    : (none)
// Returns : Stats struct (zero-initialised until the first generate() call)
// Memory  : no allocations (returns by value)
// Threads : main thread only
LayerInferenceEngine::Stats LayerInferenceEngine::stats() const {
    return impl_->last_stats_;
}

// Purpose : Free all GPU resources, CPU model data, and GGUF metadata.
// Args    : (none — destructor)
// Returns : void
// Memory  : delegates full teardown to Impl::release_all()
// Threads : main thread only
LayerInferenceEngine::~LayerInferenceEngine() {
    if (impl_) impl_->release_all();
}
