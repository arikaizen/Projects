#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// managed_buft.hpp
//
// Custom ggml buffer type backed by CUDA Unified Memory (cudaMallocManaged).
//
// Tensors in this buffer are accessible from both CPU and GPU.  The CUDA
// driver pages tensor data between system RAM and GPU VRAM on demand; when
// a CUDA kernel accesses a page that is not in VRAM, the driver fetches it
// transparently over PCIe.
//
// Integration with llama.cpp:
//   Pass managed_buft_get() in llama_model_params.tensor_buft_overrides for
//   overflow-layer tensor patterns ("blk.N.*" for N >= window_size).
//
//   Because is_host() returns true, ggml's CUDA backend treats these tensors
//   as host memory: it copies each weight tensor into a small VRAM scratch
//   buffer before running the CUDA kernel, then reuses that scratch for the
//   next tensor.  The peak VRAM used for weights is therefore ~1 layer at a
//   time, not the full model.
//
//   With n_gpu_layers=9999, ALL transformer ops run on the CUDA backend.
//   No CPU computation occurs for overflow layers.
//
// Performance:
//   Call managed_buft_prefetch(ptr, size, device) to issue a cudaMemPrefetchAsync
//   hint so overflow layer pages are already migrated to GPU when ggml copies
//   them to VRAM scratch.  This hides the PCIe latency.
// ─────────────────────────────────────────────────────────────────────────────
#include <ggml-backend.h>
#include <cstddef>

// Singleton buffer type — safe to call multiple times.
ggml_backend_buffer_type_t managed_buft_get();

// Issue an async prefetch hint for [ptr, ptr+size) to the given CUDA device.
// Pass a cudaStream_t cast to void* (or nullptr for the default stream).
void managed_buft_prefetch(const void* ptr, size_t size, int device,
                           void* cuda_stream = nullptr);
