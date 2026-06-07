// layer_streamer.cpp — double-buffered GPU layer prefetcher
#include "layer_streamer.hpp"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#ifdef __linux__
#  include <sys/fcntl.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// CUDA driver API types (used via dlopen — no #include <cuda.h> needed)
// ─────────────────────────────────────────────────────────────────────────────
using CUdeviceptr  = unsigned long long;
using CUstream     = void*;
using CUevent      = void*;
using CUresult     = int;

static constexpr int CU_SUCCESS         = 0;
static constexpr int CU_EVENT_DISABLE_TIMING = 2;

struct CudaAPI {
    void* handle{nullptr};

    // Device memory
    CUresult (*cuMemAlloc)    (CUdeviceptr*, size_t)            = nullptr;
    CUresult (*cuMemFree)     (CUdeviceptr)                     = nullptr;
    // Pinned host memory
    CUresult (*cuMemAllocHost)(void**, size_t)                  = nullptr;
    CUresult (*cuMemFreeHost) (void*)                           = nullptr;
    // Async memcpy
    CUresult (*cuMemcpyHtoDAsync)(CUdeviceptr, const void*,
                                  size_t, CUstream)             = nullptr;
    // Streams
    CUresult (*cuStreamCreate)(CUstream*, unsigned int)         = nullptr;
    CUresult (*cuStreamDestroy)(CUstream)                       = nullptr;
    CUresult (*cuStreamWaitEvent)(CUstream, CUevent, unsigned)  = nullptr;
    // Events
    CUresult (*cuEventCreate)(CUevent*, unsigned int)           = nullptr;
    CUresult (*cuEventDestroy)(CUevent)                         = nullptr;
    CUresult (*cuEventRecord)(CUevent, CUstream)                = nullptr;
    CUresult (*cuEventSynchronize)(CUevent)                     = nullptr;
    // Misc
    CUresult (*cuInit)        (unsigned int)                    = nullptr;
    CUresult (*cuCtxGetCurrent)(void**)                         = nullptr;

    bool load() {
        static const char* libs[] = {
            "libcuda.so.1", "libcuda.so", nullptr
        };
        for (const char* const* p = libs; *p && !handle; ++p)
            handle = dlopen(*p, RTLD_LAZY | RTLD_NOLOAD);
        if (!handle)
            for (const char* const* p = libs; *p && !handle; ++p)
                handle = dlopen(*p, RTLD_LAZY);
        if (!handle) return false;

        #define LOAD(fn) fn = reinterpret_cast<decltype(fn)>(dlsym(handle, #fn)); \
                         if (!fn) { dlclose(handle); handle = nullptr; return false; }
        LOAD(cuMemAlloc)
        LOAD(cuMemFree)
        LOAD(cuMemAllocHost)
        LOAD(cuMemFreeHost)
        LOAD(cuMemcpyHtoDAsync)
        LOAD(cuStreamCreate)
        LOAD(cuStreamDestroy)
        LOAD(cuStreamWaitEvent)
        LOAD(cuEventCreate)
        LOAD(cuEventDestroy)
        LOAD(cuEventRecord)
        LOAD(cuEventSynchronize)
        LOAD(cuInit)
        // cuCtxGetCurrent may not be available in all driver versions — optional
        cuCtxGetCurrent = reinterpret_cast<decltype(cuCtxGetCurrent)>(
            dlsym(handle, "cuCtxGetCurrent"));
        #undef LOAD
        return true;
    }

    void unload() {
        if (handle) { dlclose(handle); handle = nullptr; }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Impl — holds all CUDA state
// ─────────────────────────────────────────────────────────────────────────────
struct LayerStreamer::Impl {
    CudaAPI cu;

    // Two VRAM slots for double-buffering (slot 0 = active, slot 1 = prefetch)
    CUdeviceptr device_slot[2]{0, 0};
    size_t      slot_bytes{0};

    // Two CUDA events — signal when each slot's transfer is complete
    CUevent  xfer_done[2]{nullptr, nullptr};

    // Dedicated transfer stream (separate from compute so async overlap works)
    CUstream xfer_stream{nullptr};

    // Pinned host memory per CPU-resident layer
    struct PinnedLayer {
        void*  ptr{nullptr};
        size_t size{0};
    };
    std::vector<PinnedLayer> pinned;

    // Double-buffer state
    int current_slot{0};          // which slot the compute stream is reading
    int current_layer_idx{-1};    // absolute layer index in the prefetch slot

    void destroy() {
        if (!cu.handle) return;
        for (int s = 0; s < 2; ++s) {
            if (device_slot[s]) { cu.cuMemFree(device_slot[s]); device_slot[s] = 0; }
            if (xfer_done[s])   { cu.cuEventDestroy(xfer_done[s]); xfer_done[s] = nullptr; }
        }
        for (auto& pl : pinned)
            if (pl.ptr) cu.cuMemFreeHost(pl.ptr);
        pinned.clear();
        if (xfer_stream) { cu.cuStreamDestroy(xfer_stream); xfer_stream = nullptr; }
        cu.unload();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
LayerStreamer::LayerStreamer(const GGUFModelInfo& info,
                             int first_cpu_layer,
                             const std::string& gguf_path)
    : m_impl(std::make_unique<Impl>())
    , m_first_cpu_layer(first_cpu_layer)
    , m_n_cpu_layers(std::max(0, info.n_layers - first_cpu_layer))
    , m_gguf_path(gguf_path)
{
    build_layer_ranges(info);

    // Open file descriptor for fadvise (read-only, non-blocking)
    m_gguf_fd = ::open(gguf_path.c_str(), O_RDONLY);
    if (m_gguf_fd < 0)
        std::cerr << "[LayerStreamer] warning: cannot open GGUF for fadvise: "
                  << strerror(errno) << "\n";

    m_cuda_active = init_cuda(info);
    if (!m_cuda_active)
        std::cerr << "[LayerStreamer] CUDA unavailable; using fadvise-only mode\n";
}

LayerStreamer::~LayerStreamer()
{
    m_impl->destroy();
    if (m_gguf_fd >= 0) ::close(m_gguf_fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// build_layer_ranges — compute file byte ranges for each CPU-resident layer
// ─────────────────────────────────────────────────────────────────────────────
void LayerStreamer::build_layer_ranges(const GGUFModelInfo& info)
{
    m_layer_ranges.resize(static_cast<size_t>(m_n_cpu_layers), {0, 0});

    for (const auto& ti : info.tensors) {
        // Determine layer index
        int layer = -1;
        if (ti.name.size() >= 5 && ti.name[0] == 'b' && ti.name[1] == 'l'
                                 && ti.name[2] == 'k' && ti.name[3] == '.') {
            size_t dot = ti.name.find('.', 4);
            if (dot != std::string::npos) {
                try { layer = std::stoi(ti.name.substr(4, dot - 4)); }
                catch (...) {}
            }
        }
        if (layer < m_first_cpu_layer || layer >= (m_first_cpu_layer + m_n_cpu_layers))
            continue;

        int rel = layer - m_first_cpu_layer;
        uint64_t abs_off = info.data_section_start + ti.data_offset;
        auto& lr = m_layer_ranges[static_cast<size_t>(rel)];
        if (lr.length == 0) {
            lr.offset = abs_off;
            lr.length = ti.size_bytes;
        } else {
            // Expand range to cover all tensors in this layer contiguously
            uint64_t cur_end = lr.offset + lr.length;
            uint64_t new_end = abs_off + ti.size_bytes;
            lr.offset = std::min(lr.offset, abs_off);
            lr.length = static_cast<size_t>(std::max(cur_end, new_end) - lr.offset);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// init_cuda — allocate VRAM slots, pinned host buffers, events, stream
// ─────────────────────────────────────────────────────────────────────────────
bool LayerStreamer::init_cuda(const GGUFModelInfo& info)
{
    auto& cu = m_impl->cu;
    if (!cu.load()) return false;

    // Initialise driver; returns non-zero if no GPU present.
    if (cu.cuInit(0) != CU_SUCCESS) return false;

    // Determine the largest CPU-resident layer (slots must fit any layer)
    size_t max_layer_bytes = 0;
    for (int l = m_first_cpu_layer; l < info.n_layers; ++l)
        max_layer_bytes = std::max(max_layer_bytes,
            info.layer_bytes[static_cast<size_t>(l)]);
    if (max_layer_bytes == 0) return false;

    m_slot_bytes        = max_layer_bytes;
    m_impl->slot_bytes  = max_layer_bytes;

    // Allocate two VRAM slots
    for (int s = 0; s < 2; ++s) {
        if (cu.cuMemAlloc(&m_impl->device_slot[s], max_layer_bytes) != CU_SUCCESS) {
            std::cerr << "[LayerStreamer] failed to allocate VRAM slot " << s << "\n";
            return false;
        }
    }

    // Allocate pinned host memory for each CPU-resident layer
    m_impl->pinned.resize(static_cast<size_t>(m_n_cpu_layers));
    m_pinned_total = 0;
    for (int r = 0; r < m_n_cpu_layers; ++r) {
        size_t sz = info.layer_bytes[static_cast<size_t>(m_first_cpu_layer + r)];
        void* ptr = nullptr;
        if (cu.cuMemAllocHost(&ptr, sz) != CU_SUCCESS) {
            std::cerr << "[LayerStreamer] failed to allocate pinned memory for layer "
                      << (m_first_cpu_layer + r) << "\n";
            return false;
        }
        m_impl->pinned[static_cast<size_t>(r)] = {ptr, sz};
        m_pinned_total += sz;
    }

    // Allocate transfer stream
    if (cu.cuStreamCreate(&m_impl->xfer_stream, 0) != CU_SUCCESS) {
        std::cerr << "[LayerStreamer] failed to create transfer stream\n";
        return false;
    }

    // Allocate two completion events (no timing — lower overhead)
    for (int s = 0; s < 2; ++s) {
        if (cu.cuEventCreate(&m_impl->xfer_done[s],
                             static_cast<unsigned>(CU_EVENT_DISABLE_TIMING))
                != CU_SUCCESS) {
            std::cerr << "[LayerStreamer] failed to create CUDA event\n";
            return false;
        }
    }

    // Pre-load all CPU-resident layers into pinned host memory from GGUF file.
    // This happens once at startup; subsequent passes reuse the pinned buffers.
    {
        int fd = ::open(m_gguf_path.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cerr << "[LayerStreamer] cannot open GGUF to pre-load layers: "
                      << strerror(errno) << "\n";
            return false;
        }

        for (int r = 0; r < m_n_cpu_layers; ++r) {
            const auto& lr  = m_layer_ranges[static_cast<size_t>(r)];
            auto&       pl  = m_impl->pinned[static_cast<size_t>(r)];
            if (lr.length == 0 || lr.length > pl.size) continue;

            // Read tensor data into pinned host buffer
            ssize_t got = ::pread(fd, pl.ptr,
                                  static_cast<size_t>(lr.length),
                                  static_cast<off_t>(lr.offset));
            if (got != static_cast<ssize_t>(lr.length))
                std::cerr << "[LayerStreamer] warning: partial read for layer "
                          << (m_first_cpu_layer + r) << "\n";
        }
        ::close(fd);
    }

    std::cerr << "[LayerStreamer] CUDA double-buffer ready: "
              << m_n_cpu_layers << " overflow layers, "
              << (m_pinned_total >> 20) << " MiB pinned, "
              << (max_layer_bytes >> 20) << " MiB/slot\n";

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page-cache prefetch (always available)
// ─────────────────────────────────────────────────────────────────────────────
void LayerStreamer::prefetch_for_llama(int layer_idx)
{
    int rel = layer_idx - m_first_cpu_layer;
    if (rel < 0 || rel >= m_n_cpu_layers) return;
    if (m_gguf_fd < 0) return;

    const auto& lr = m_layer_ranges[static_cast<size_t>(rel)];
    if (lr.length == 0) return;

#ifdef __linux__
    // POSIX_FADV_WILLNEED: tell the kernel to start reading these pages now.
    ::posix_fadvise(m_gguf_fd,
                    static_cast<off_t>(lr.offset),
                    static_cast<off_t>(lr.length),
                    POSIX_FADV_WILLNEED);
#endif
}

void LayerStreamer::prefetch_all()
{
    for (int l = m_first_cpu_layer; l < (m_first_cpu_layer + m_n_cpu_layers); ++l)
        prefetch_for_llama(l);
}

// ─────────────────────────────────────────────────────────────────────────────
// CUDA double-buffer pass interface
// ─────────────────────────────────────────────────────────────────────────────
void LayerStreamer::begin_pass()
{
    if (!m_cuda_active) return;
    auto& im = *m_impl;
    auto& cu = im.cu;

    im.current_slot      = 0;
    im.current_layer_idx = m_first_cpu_layer;

    // Kick off async transfer of first layer into slot 0.
    int rel0 = 0;
    auto& pl0 = im.pinned[static_cast<size_t>(rel0)];
    cu.cuMemcpyHtoDAsync(im.device_slot[0], pl0.ptr, pl0.size, im.xfer_stream);
    cu.cuEventRecord(im.xfer_done[0], im.xfer_stream);

    // Also prefetch second layer into slot 1 if it exists.
    if (m_n_cpu_layers > 1) {
        auto& pl1 = im.pinned[1];
        cu.cuMemcpyHtoDAsync(im.device_slot[1], pl1.ptr, pl1.size, im.xfer_stream);
        cu.cuEventRecord(im.xfer_done[1], im.xfer_stream);
    }
}

void* LayerStreamer::wait_ready(int layer_idx)
{
    if (!m_cuda_active) return nullptr;
    auto& im = *m_impl;
    auto& cu = im.cu;

    int slot = im.current_slot;
    // Block until the transfer for this slot is complete.
    cu.cuEventSynchronize(im.xfer_done[slot]);

    int rel_next = (layer_idx - m_first_cpu_layer) + 1;
    int next_slot = 1 - slot;

    // Launch async prefetch of the next layer into the other slot.
    if (rel_next < m_n_cpu_layers) {
        auto& pl = im.pinned[static_cast<size_t>(rel_next)];
        cu.cuMemcpyHtoDAsync(im.device_slot[next_slot], pl.ptr, pl.size, im.xfer_stream);
        cu.cuEventRecord(im.xfer_done[next_slot], im.xfer_stream);
        im.current_layer_idx = m_first_cpu_layer + rel_next;
    }

    return reinterpret_cast<void*>(im.device_slot[slot]);
}

void LayerStreamer::release_current()
{
    if (!m_cuda_active) return;
    m_impl->current_slot = 1 - m_impl->current_slot;
}

void LayerStreamer::end_pass()
{
    if (!m_cuda_active) return;
    m_impl->current_slot      = 0;
    m_impl->current_layer_idx = -1;
}
