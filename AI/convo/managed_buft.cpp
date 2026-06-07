// managed_buft.cpp — ggml buffer type backed by CUDA Unified (Managed) Memory
//
// Requires: ggml-backend.h, ggml.h, cuda_runtime.h
// Build note: $(LLAMA_DIR)/ggml/include must be in include path (already in Makefile).
// ─────────────────────────────────────────────────────────────────────────────
#include "managed_buft.hpp"

#include <ggml-backend.h>
#include <ggml.h>
#include <cuda_runtime.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Internal buffer context
// ─────────────────────────────────────────────────────────────────────────────
struct ManagedBufCtx {
    void*  base{nullptr};
    size_t size{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// Buffer vtable implementations
// ─────────────────────────────────────────────────────────────────────────────
static void managed_buffer_free(ggml_backend_buffer_t buf)
{
    auto* ctx = static_cast<ManagedBufCtx*>(buf->context);
    if (ctx->base) {
        cudaFree(ctx->base);
        ctx->base = nullptr;
    }
    delete ctx;
}

static void* managed_buffer_get_base(ggml_backend_buffer_t buf)
{
    return static_cast<ManagedBufCtx*>(buf->context)->base;
}

static void managed_buffer_init_tensor(ggml_backend_buffer_t, struct ggml_tensor*)
{
    // ggml allocator already set tensor->data = base + offset before this call.
}

static void managed_buffer_memset_tensor(ggml_backend_buffer_t,
                                          struct ggml_tensor* t,
                                          uint8_t val, size_t offset, size_t sz)
{
    // cudaMemset works on managed memory from CPU side.
    std::memset(static_cast<char*>(t->data) + offset, val, sz);
}

static void managed_buffer_set_tensor(ggml_backend_buffer_t,
                                       struct ggml_tensor* t,
                                       const void* data, size_t offset, size_t sz)
{
    // Managed memory is CPU-accessible; plain memcpy is correct.
    std::memcpy(static_cast<char*>(t->data) + offset, data, sz);
}

static void managed_buffer_get_tensor(ggml_backend_buffer_t,
                                       const struct ggml_tensor* t,
                                       void* data, size_t offset, size_t sz)
{
    std::memcpy(data, static_cast<const char*>(t->data) + offset, sz);
}

static bool managed_buffer_cpy_tensor(ggml_backend_buffer_t,
                                       const struct ggml_tensor* src,
                                       struct ggml_tensor* dst)
{
    if (src->data && dst->data) {
        // cudaMemcpy with cudaMemcpyDefault works for managed↔anything.
        cudaMemcpy(dst->data, src->data, ggml_nbytes(src), cudaMemcpyDefault);
        return true;
    }
    return false;
}

static void managed_buffer_clear(ggml_backend_buffer_t buf, uint8_t val)
{
    auto* ctx = static_cast<ManagedBufCtx*>(buf->context);
    std::memset(ctx->base, val, ctx->size);
}

// Buffer vtable — field order matches ggml_backend_buffer_i in ggml-backend.h
static const ggml_backend_buffer_i managed_buffer_iface = {
    /* free_buffer    */ managed_buffer_free,
    /* get_base       */ managed_buffer_get_base,
    /* init_tensor    */ managed_buffer_init_tensor,
    /* memset_tensor  */ managed_buffer_memset_tensor,
    /* set_tensor     */ managed_buffer_set_tensor,
    /* get_tensor     */ managed_buffer_get_tensor,
    /* cpy_tensor     */ managed_buffer_cpy_tensor,
    /* clear          */ managed_buffer_clear,
    /* reset          */ nullptr,
};

// ─────────────────────────────────────────────────────────────────────────────
// Buffer type vtable implementations
// ─────────────────────────────────────────────────────────────────────────────
static const char* managed_buft_name(ggml_backend_buffer_type_t)
{
    return "CUDA-Managed";
}

static ggml_backend_buffer_t managed_buft_alloc(ggml_backend_buffer_type_t buft,
                                                 size_t size)
{
    void* ptr = nullptr;
    cudaError_t err = cudaMallocManaged(&ptr, size, cudaMemAttachGlobal);
    if (err != cudaSuccess) {
        std::cerr << "[managed_buft] cudaMallocManaged(" << size
                  << ") failed: " << cudaGetErrorString(err) << "\n";
        return nullptr;
    }

    // Prefer this allocation on the GPU.  CPU can still read/write (for weight loading).
    cudaMemAdvise(ptr, size, cudaMemAdviseSetPreferredLocation, 0 /*device 0*/);
    cudaMemAdvise(ptr, size, cudaMemAdviseSetAccessedBy,       cudaCpuDeviceId);
    cudaMemAdvise(ptr, size, cudaMemAdviseSetAccessedBy,       0 /*device 0*/);

    auto* ctx = new ManagedBufCtx{ptr, size};
    return ggml_backend_buffer_init(buft, managed_buffer_iface, ctx, size);
}

static size_t managed_buft_get_alignment(ggml_backend_buffer_type_t)
{
    return 128; // CUDA alignment requirement
}

static size_t managed_buft_get_max_size(ggml_backend_buffer_type_t)
{
    return SIZE_MAX;
}

static bool managed_buft_is_host(ggml_backend_buffer_type_t)
{
    // Returning true makes ggml's CUDA backend copy these tensors to VRAM
    // scratch before kernels run.  The CUDA backend supports host buffers,
    // ensuring ALL compute stays on the GPU.
    return true;
}

// Buffer type vtable — field order matches ggml_backend_buffer_type_i
static const ggml_backend_buffer_type_i managed_buft_iface = {
    /* get_name       */ managed_buft_name,
    /* alloc_buffer   */ managed_buft_alloc,
    /* get_alignment  */ managed_buft_get_alignment,
    /* get_max_size   */ managed_buft_get_max_size,
    /* get_alloc_size */ nullptr,   // use ggml_nbytes
    /* is_host        */ managed_buft_is_host,
};

static ggml_backend_buffer_type managed_buft_singleton = {
    /* iface   */ managed_buft_iface,
    /* device  */ nullptr,  // host-type buffer; no device association needed
    /* context */ nullptr,
};

ggml_backend_buffer_type_t managed_buft_get()
{
    return &managed_buft_singleton;
}

// ─────────────────────────────────────────────────────────────────────────────
// Prefetch helper
// ─────────────────────────────────────────────────────────────────────────────
void managed_buft_prefetch(const void* ptr, size_t size, int device, void* cuda_stream)
{
    if (!ptr || size == 0) return;
    cudaStream_t stream = static_cast<cudaStream_t>(cuda_stream);
    cudaError_t  err    = cudaMemPrefetchAsync(ptr, size, device, stream);
    if (err != cudaSuccess && err != cudaErrorInvalidDevice)
        std::cerr << "[managed_buft] prefetch warning: " << cudaGetErrorString(err) << "\n";
}
