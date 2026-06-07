#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// gguf_planner.hpp
//
// Lightweight GGUF file-header reader and VRAM planner.
//
// Reads just the GGUF metadata section (first few MB at most) — the model
// weights are NOT loaded — to determine:
//   • per-layer tensor byte sizes
//   • total model footprint
// Then queries the CUDA device for available VRAM and computes the highest
// n_gpu_layers that fits while leaving a configurable headroom.
//
// No dependency on llama.cpp or CUDA headers; CUDA is queried at runtime
// through dlopen so the code compiles on machines without a GPU.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// GGML quantisation types — mirrors ggml.h enum values so we can compute
// bytes-per-element without including ggml headers.
// ─────────────────────────────────────────────────────────────────────────────
enum class GGMLType : int32_t {
    F32   = 0,
    F16   = 1,
    Q4_0  = 2,
    Q4_1  = 3,
    Q8_0  = 8,
    Q2_K  = 10,
    Q3_K  = 11,
    Q4_K  = 12,
    Q5_K  = 13,
    Q6_K  = 14,
    IQ2_XXS = 19,
    IQ2_XS  = 20,
    IQ3_XXS = 23,
    BF16  = 30,
    UNKNOWN = -1,
};

// Returns bytes per element (fractional for block quantisation).
double ggml_type_bytes_per_element(GGMLType t);

// ─────────────────────────────────────────────────────────────────────────────
// Per-tensor info parsed from the GGUF tensor-info section.
// ─────────────────────────────────────────────────────────────────────────────
struct GGUFTensorInfo {
    std::string            name;
    std::vector<uint64_t>  dims;           // shape in GGUF storage order
    GGMLType               type{GGMLType::F32};
    size_t                 size_bytes{0};
    uint64_t               data_offset{0}; // offset from data section start
};

// ─────────────────────────────────────────────────────────────────────────────
// Summary of the model's structure and size, extracted from the GGUF header.
// ─────────────────────────────────────────────────────────────────────────────
struct GGUFModelInfo {
    uint32_t version{0};

    // Architecture metadata
    int32_t  n_layers{0};    // llama.block_count
    int32_t  n_embd{0};      // llama.embedding_length
    int32_t  n_head{0};      // llama.attention.head_count
    int32_t  n_head_kv{0};   // llama.attention.head_count_kv (GQA)
    int32_t  n_ff{0};        // llama.feed_forward_length

    // Size breakdown
    std::vector<size_t>  layer_bytes;        // [n_layers] bytes per transformer layer
    size_t               non_layer_bytes{0}; // embedding table + output head
    size_t               total_bytes{0};

    // File layout (needed by LayerStreamer for direct data access)
    uint64_t data_section_start{0}; // absolute file byte position of the data section
    uint32_t alignment{32};         // GGUF alignment (general.alignment KV, default 32)

    // All tensors (for detailed inspection)
    std::vector<GGUFTensorInfo> tensors;
};

// Parse the GGUF file header and return model structure info.
// Throws std::runtime_error on format errors.
// Fast: reads only the header / metadata / tensor-info sections, not the weights.
GGUFModelInfo gguf_read_metadata(const std::string& path);

// ─────────────────────────────────────────────────────────────────────────────
// VRAM query (CUDA optional)
// ─────────────────────────────────────────────────────────────────────────────

// Returns free VRAM on device 0 in bytes.  Returns 0 if no CUDA device found.
size_t vram_query_free();

// Returns total VRAM on device 0 in bytes.  Returns 0 if no CUDA device found.
size_t vram_query_total();

// ─────────────────────────────────────────────────────────────────────────────
// VRAM plan
// ─────────────────────────────────────────────────────────────────────────────
struct VRAMPlan {
    int    n_gpu_layers{0};           // recommended llama n_gpu_layers value
    int    n_total_layers{0};         // total transformer layers in model
    bool   fits_fully{false};         // true → whole model fits, no streaming needed
    bool   needs_streaming{false};    // true → overflow layers will stream via CPU

    size_t vram_free_bytes{0};        // free VRAM at planning time
    size_t vram_layer_bytes{0};       // VRAM consumed by GPU layers
    size_t vram_kv_cache_bytes{0};    // estimated KV cache footprint
    size_t vram_headroom_bytes{0};    // reserved free VRAM after placement
    size_t vram_after_bytes{0};       // free VRAM remaining after plan

    size_t cpu_layer_bytes{0};        // bytes that will stay on CPU / be streamed
};

// Compute an optimal VRAMPlan.
//
// context_length  — token context window (used to size KV cache estimate)
// headroom_bytes  — VRAM to keep free for OS / driver / other processes.
//                   Default: 512 MiB.  Raise it if you see OOM at runtime.
// kv_type_bytes   — bytes per KV cache element (2 = fp16, 4 = fp32)
VRAMPlan plan_vram(
    const GGUFModelInfo& info,
    size_t context_length  = 4096,
    size_t headroom_bytes  = 512ULL * 1024 * 1024,
    size_t kv_type_bytes   = 2
);

// Pretty-print a VRAMPlan to stderr for diagnostic purposes.
void vram_plan_print(const VRAMPlan& plan, const std::string& model_path = {});
