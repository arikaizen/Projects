#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// layer_streamer.hpp
//
// Double-buffered GPU layer prefetcher for models that partially overflow VRAM.
//
// When n_gpu_layers < n_total_layers, the CPU-resident transformer layers are
// streamed one at a time from the GGUF file.  This class manages:
//
//   • Pinned (page-locked) host memory for all overflow-layer tensor data.
//     Pinned memory enables full PCIe DMA bandwidth without CPU involvement.
//
//   • Two VRAM slots (slot A, slot B) sized for the largest overflow layer.
//     While slot A is "active" (in use by CUDA compute), slot B is being
//     filled asynchronously with the NEXT layer's weights via cudaMemcpyAsync
//     on a dedicated transfer stream.
//
//   • CUDA events to synchronise transfer completion before compute starts.
//
//   • POSIX fadvise prefetch for the mmap-paged GGUF regions that llama.cpp
//     accesses, reducing latency for CPU-resident layers even when CUDA is
//     unavailable.
//
// Integration note:
//   llama.cpp manages its own tensor buffers; this class cannot intercept
//   llama.cpp's own layer dispatch.  Its primary use is:
//     (a) Kernel read-ahead via prefetch_for_llama() so CPU layers are in
//         page-cache before llama.cpp touches them.
//     (b) Stand-alone CUDA double-buffer streaming for custom inference
//         engines that read raw quantised weights directly.
//
// CUDA is optional: if no CUDA device is found the class falls back to
// POSIX-fadvise-only mode and is_active() returns false.
// ─────────────────────────────────────────────────────────────────────────────
#include "gguf_planner.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class LayerStreamer {
public:
    // Construct the streamer.
    //
    // info           — metadata returned by gguf_read_metadata()
    // first_cpu_layer — index of first CPU-resident layer (= plan.n_gpu_layers)
    // gguf_path      — path to the GGUF file (tensor data is read from here)
    LayerStreamer(const GGUFModelInfo& info,
                  int first_cpu_layer,
                  const std::string& gguf_path);
    ~LayerStreamer();

    LayerStreamer(const LayerStreamer&)            = delete;
    LayerStreamer& operator=(const LayerStreamer&) = delete;

    // Returns true when CUDA double-buffering is active.
    // Returns false when running in POSIX-fadvise-only fallback mode.
    bool is_active() const noexcept { return m_cuda_active; }

    // ── Page-cache prefetch (always available, no CUDA required) ─────────────

    // Issue fadvise(WILLNEED) for the GGUF file region of the given layer.
    // Call this for layer N+1 while computing layer N to hide I/O latency.
    void prefetch_for_llama(int layer_idx);

    // Prefetch all CPU-resident layers at once (useful during model warmup).
    void prefetch_all();

    // ── CUDA double-buffer streaming interface ────────────────────────────────
    // (No-ops when is_active() == false)

    // Begin a forward pass.  Loads layer first_cpu_layer into slot A and starts
    // async prefetch of layer first_cpu_layer+1 into slot B.
    void begin_pass();

    // Block until layer_idx is ready in the current VRAM slot.
    // Simultaneously launches async transfer of (layer_idx+1) into the other slot.
    // Returns device pointer to the layer's weight data (null in fallback mode).
    void* wait_ready(int layer_idx);

    // Signal that the current slot is no longer used by the compute stream.
    // Swaps active/prefetch slots.
    void release_current();

    // End the pass and reset slot state.
    void end_pass();

    // ── Diagnostics ──────────────────────────────────────────────────────────

    // Total bytes in pinned host memory (CPU-resident layer weights).
    size_t pinned_bytes() const noexcept { return m_pinned_total; }

    // Size of one VRAM slot (= largest overflow layer).
    size_t slot_bytes()   const noexcept { return m_slot_bytes; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool   m_cuda_active{false};
    size_t m_pinned_total{0};
    size_t m_slot_bytes{0};

    int                 m_first_cpu_layer{0};
    int                 m_n_cpu_layers{0};
    std::string         m_gguf_path;
    int                 m_gguf_fd{-1};          // file descriptor for fadvise

    // Per-layer file ranges for fadvise (absolute byte offsets)
    struct LayerRange { uint64_t offset; size_t length; };
    std::vector<LayerRange> m_layer_ranges;

    void build_layer_ranges(const GGUFModelInfo& info);
    bool init_cuda(const GGUFModelInfo& info);
};
