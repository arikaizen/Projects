// gguf_planner.cpp — GGUF metadata reader + VRAM planner implementation
#include "gguf_planner.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// GGUF binary format constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t GGUF_MAGIC = 0x46554747u; // "GGUF" little-endian

enum class GGUFValueType : uint32_t {
    UINT8   = 0,
    INT8    = 1,
    UINT16  = 2,
    INT16   = 3,
    UINT32  = 4,
    INT32   = 5,
    FLOAT32 = 6,
    BOOL    = 7,
    STRING  = 8,
    ARRAY   = 9,
    UINT64  = 10,
    INT64   = 11,
    FLOAT64 = 12,
};

// ─────────────────────────────────────────────────────────────────────────────
// Minimal little-endian binary stream reader
// ─────────────────────────────────────────────────────────────────────────────
class BinReader {
    std::ifstream m_f;
public:
    explicit BinReader(const std::string& path)
        : m_f(path, std::ios::binary)
    {
        if (!m_f)
            throw std::runtime_error("gguf_planner: cannot open \"" + path + "\"");
    }

    template<typename T>
    T read() {
        T v{};
        m_f.read(reinterpret_cast<char*>(&v), sizeof v);
        if (!m_f)
            throw std::runtime_error("gguf_planner: unexpected end of file");
        return v;
    }

    std::string read_str() {
        uint64_t len = read<uint64_t>();
        if (len > 256ULL * 1024 * 1024)
            throw std::runtime_error("gguf_planner: implausibly long string in header");
        std::string s(static_cast<size_t>(len), '\0');
        if (len > 0)
            m_f.read(s.data(), static_cast<std::streamsize>(len));
        if (!m_f)
            throw std::runtime_error("gguf_planner: unexpected end of file in string");
        return s;
    }

    // Current file position
    uint64_t tell() {
        return static_cast<uint64_t>(m_f.tellg());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Skip a GGUF value (recursive for ARRAY)
// ─────────────────────────────────────────────────────────────────────────────
static void skip_value(BinReader& r, GGUFValueType vt)
{
    switch (vt) {
        case GGUFValueType::UINT8:   r.read<uint8_t>();   break;
        case GGUFValueType::INT8:    r.read<int8_t>();    break;
        case GGUFValueType::UINT16:  r.read<uint16_t>();  break;
        case GGUFValueType::INT16:   r.read<int16_t>();   break;
        case GGUFValueType::UINT32:  r.read<uint32_t>();  break;
        case GGUFValueType::INT32:   r.read<int32_t>();   break;
        case GGUFValueType::FLOAT32: r.read<float>();     break;
        case GGUFValueType::BOOL:    r.read<uint8_t>();   break;
        case GGUFValueType::STRING:  r.read_str();        break;
        case GGUFValueType::UINT64:  r.read<uint64_t>();  break;
        case GGUFValueType::INT64:   r.read<int64_t>();   break;
        case GGUFValueType::FLOAT64: r.read<double>();    break;
        case GGUFValueType::ARRAY: {
            auto arr_type = static_cast<GGUFValueType>(r.read<uint32_t>());
            uint64_t count = r.read<uint64_t>();
            for (uint64_t i = 0; i < count; ++i)
                skip_value(r, arr_type);
            break;
        }
        default:
            throw std::runtime_error("gguf_planner: unknown GGUF value type "
                + std::to_string(static_cast<uint32_t>(vt)));
    }
}

// Read a scalar integer value; skip and return -1 for non-integer types.
static int64_t read_int_or_skip(BinReader& r, GGUFValueType vt)
{
    switch (vt) {
        case GGUFValueType::UINT8:  return r.read<uint8_t>();
        case GGUFValueType::INT8:   return r.read<int8_t>();
        case GGUFValueType::UINT16: return r.read<uint16_t>();
        case GGUFValueType::INT16:  return r.read<int16_t>();
        case GGUFValueType::UINT32: return r.read<uint32_t>();
        case GGUFValueType::INT32:  return r.read<int32_t>();
        case GGUFValueType::UINT64: return static_cast<int64_t>(r.read<uint64_t>());
        case GGUFValueType::INT64:  return r.read<int64_t>();
        default:
            skip_value(r, vt);
            return -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ggml_type_bytes_per_element  (exported, used by layer_streamer too)
// ─────────────────────────────────────────────────────────────────────────────
double ggml_type_bytes_per_element(GGMLType t)
{
    switch (t) {
        case GGMLType::F32:     return 4.0;
        case GGMLType::F16:     return 2.0;
        case GGMLType::BF16:    return 2.0;
        case GGMLType::Q4_0:    return 18.0 / 32.0;
        case GGMLType::Q4_1:    return 20.0 / 32.0;
        case GGMLType::Q8_0:    return 34.0 / 32.0;
        case GGMLType::Q2_K:    return 84.0  / 256.0;
        case GGMLType::Q3_K:    return 110.0 / 256.0;
        case GGMLType::Q4_K:    return 144.0 / 256.0;
        case GGMLType::Q5_K:    return 176.0 / 256.0;
        case GGMLType::Q6_K:    return 210.0 / 256.0;
        case GGMLType::IQ2_XXS: return 66.0  / 256.0;
        case GGMLType::IQ2_XS:  return 70.0  / 256.0;
        case GGMLType::IQ3_XXS: return 98.0  / 256.0;
        default:                return 4.0;
    }
}

// Byte size of a tensor using exact block-quantisation arithmetic.
static size_t tensor_byte_size(GGMLType type, const std::vector<uint64_t>& dims)
{
    uint64_t n_elems = 1;
    for (auto d : dims) n_elems *= d;

    struct BP { uint32_t block_size, bytes_per_block; };
    static const std::map<GGMLType, BP> bp_map = {
        {GGMLType::F32,     {1,   4}},
        {GGMLType::F16,     {1,   2}},
        {GGMLType::BF16,    {1,   2}},
        {GGMLType::Q4_0,    {32,  18}},
        {GGMLType::Q4_1,    {32,  20}},
        {GGMLType::Q8_0,    {32,  34}},
        {GGMLType::Q2_K,    {256, 84}},
        {GGMLType::Q3_K,    {256, 110}},
        {GGMLType::Q4_K,    {256, 144}},
        {GGMLType::Q5_K,    {256, 176}},
        {GGMLType::Q6_K,    {256, 210}},
        {GGMLType::IQ2_XXS, {256, 66}},
        {GGMLType::IQ2_XS,  {256, 70}},
        {GGMLType::IQ3_XXS, {256, 98}},
    };

    auto it = bp_map.find(type);
    if (it == bp_map.end())
        return static_cast<size_t>(n_elems) * 4; // conservative unknown-type fallback

    const auto& bp = it->second;
    uint64_t n_blocks = (n_elems + bp.block_size - 1) / bp.block_size;
    return static_cast<size_t>(n_blocks) * bp.bytes_per_block;
}

// Extract transformer layer index from tensor name, e.g. "blk.7.attn_q.weight" → 7.
// Returns -1 for non-layer tensors (embeddings, output head, etc.).
static int extract_layer_index(const std::string& name)
{
    if (name.size() < 5 || name[0] != 'b' || name[1] != 'l'
                        || name[2] != 'k' || name[3] != '.')
        return -1;
    size_t dot = name.find('.', 4);
    if (dot == std::string::npos) return -1;
    try {
        int idx = std::stoi(name.substr(4, dot - 4));
        return idx >= 0 ? idx : -1;
    } catch (...) {
        return -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// gguf_read_metadata
// ─────────────────────────────────────────────────────────────────────────────
GGUFModelInfo gguf_read_metadata(const std::string& path)
{
    BinReader r(path);
    GGUFModelInfo info;

    // ── File header ───────────────────────────────────────────────────────────
    uint32_t magic = r.read<uint32_t>();
    if (magic != GGUF_MAGIC)
        throw std::runtime_error("gguf_planner: \"" + path + "\" is not a GGUF file");

    info.version = r.read<uint32_t>();
    if (info.version < 1 || info.version > 3)
        throw std::runtime_error("gguf_planner: unsupported GGUF version "
            + std::to_string(info.version));

    uint64_t n_tensors = r.read<uint64_t>();
    uint64_t n_kv      = r.read<uint64_t>();

    // ── Key-Value metadata ────────────────────────────────────────────────────
    for (uint64_t i = 0; i < n_kv; ++i) {
        std::string key = r.read_str();
        auto vt = static_cast<GGUFValueType>(r.read<uint32_t>());

        // Capture architecture integers; all others read_int_or_skip handles.
        if (key == "general.alignment") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.alignment = static_cast<uint32_t>(v);
        } else if (key == "llama.block_count") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.n_layers = static_cast<int32_t>(v);
        } else if (key == "llama.embedding_length") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.n_embd = static_cast<int32_t>(v);
        } else if (key == "llama.attention.head_count") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.n_head = static_cast<int32_t>(v);
        } else if (key == "llama.attention.head_count_kv") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.n_head_kv = static_cast<int32_t>(v);
        } else if (key == "llama.feed_forward_length") {
            int64_t v = read_int_or_skip(r, vt);
            if (v > 0) info.n_ff = static_cast<int32_t>(v);
        } else {
            skip_value(r, vt);
        }
    }

    // ── Tensor-info section ───────────────────────────────────────────────────
    info.tensors.reserve(static_cast<size_t>(n_tensors));

    for (uint64_t i = 0; i < n_tensors; ++i) {
        GGUFTensorInfo ti;
        ti.name        = r.read_str();
        uint32_t ndim  = r.read<uint32_t>();
        ti.dims.resize(ndim);
        for (uint32_t d = 0; d < ndim; ++d)
            ti.dims[d] = r.read<uint64_t>();
        ti.type        = static_cast<GGMLType>(r.read<uint32_t>());
        ti.data_offset = r.read<uint64_t>();
        ti.size_bytes  = tensor_byte_size(ti.type, ti.dims);
        info.tensors.push_back(std::move(ti));
    }

    // ── Compute data-section start (align current position to info.alignment) ─
    uint64_t pos = r.tell();
    uint64_t align = info.alignment > 0 ? info.alignment : 32u;
    info.data_section_start = (pos + align - 1) & ~(align - 1);

    // ── Aggregate layer / non-layer byte counts ───────────────────────────────
    if (info.n_layers > 0)
        info.layer_bytes.assign(static_cast<size_t>(info.n_layers), 0);

    for (const auto& ti : info.tensors) {
        int layer = extract_layer_index(ti.name);
        if (layer >= 0 && layer < info.n_layers)
            info.layer_bytes[static_cast<size_t>(layer)] += ti.size_bytes;
        else
            info.non_layer_bytes += ti.size_bytes;
        info.total_bytes += ti.size_bytes;
    }

    return info;
}

// ─────────────────────────────────────────────────────────────────────────────
// VRAM query — via CUDA driver API (dlopen, no hard dependency)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

bool query_cuda_mem(size_t& free_bytes, size_t& total_bytes)
{
    // Candidates in preference order; prefer already-loaded libraries.
    static const char* candidates[] = {
        "libcuda.so.1", "libcuda.so",
        "libcudart.so.12", "libcudart.so.11", "libcudart.so",
        nullptr
    };

    void* handle = nullptr;
    for (const char* const* p = candidates; *p && !handle; ++p)
        handle = dlopen(*p, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle)
        for (const char* const* p = candidates; *p && !handle; ++p)
            handle = dlopen(*p, RTLD_LAZY);
    if (!handle) return false;

    // Try CUDA driver API first (cuMemGetInfo_v2).
    using fn_cuInit       = int(*)(unsigned int);
    using fn_cuMemGetInfo = int(*)(size_t*, size_t*);

    auto cu_init = reinterpret_cast<fn_cuInit>(dlsym(handle, "cuInit"));
    auto cu_mgi  = reinterpret_cast<fn_cuMemGetInfo>(dlsym(handle, "cuMemGetInfo_v2"));
    if (!cu_mgi)
        cu_mgi = reinterpret_cast<fn_cuMemGetInfo>(dlsym(handle, "cuMemGetInfo"));

    if (cu_init && cu_mgi) {
        cu_init(0); // no-op if already initialised; returns non-zero if no GPU
        if (cu_mgi(&free_bytes, &total_bytes) == 0) {
            dlclose(handle);
            return true;
        }
    }

    // Fall back to CUDA runtime API (cudaMemGetInfo).
    using fn_cudaMemGetInfo = int(*)(size_t*, size_t*);
    auto cuda_mgi = reinterpret_cast<fn_cudaMemGetInfo>(
        dlsym(handle, "cudaMemGetInfo"));
    if (cuda_mgi && cuda_mgi(&free_bytes, &total_bytes) == 0) {
        dlclose(handle);
        return true;
    }

    dlclose(handle);
    return false;
}

} // anonymous namespace

size_t vram_query_free()
{
    size_t f = 0, t = 0;
    query_cuda_mem(f, t);
    return f;
}

size_t vram_query_total()
{
    size_t f = 0, t = 0;
    query_cuda_mem(f, t);
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
// plan_vram
// ─────────────────────────────────────────────────────────────────────────────
VRAMPlan plan_vram(const GGUFModelInfo& info,
                   size_t context_length,
                   size_t headroom_bytes,
                   size_t kv_type_bytes)
{
    VRAMPlan plan;
    plan.n_total_layers  = info.n_layers;
    plan.vram_free_bytes = vram_query_free();

    if (plan.vram_free_bytes == 0 || info.n_layers == 0) {
        plan.n_gpu_layers = 0;
        for (size_t b : info.layer_bytes) plan.cpu_layer_bytes += b;
        return plan;
    }

    // KV-cache bytes per GPU layer:
    //   2 (K+V) * ctx_len * head_dim * n_head_kv * kv_type_bytes
    int32_t n_head    = info.n_head    > 0 ? info.n_head    : 32;
    int32_t n_head_kv = info.n_head_kv > 0 ? info.n_head_kv : n_head;
    int32_t head_dim  = info.n_embd    > 0 ? info.n_embd / n_head : 128;
    size_t  kv_per_layer = 2
        * static_cast<size_t>(context_length)
        * static_cast<size_t>(head_dim)
        * static_cast<size_t>(n_head_kv)
        * kv_type_bytes;

    // Non-layer tensors (token embeddings, output head) also live in VRAM.
    // Budget = free VRAM - headroom - non-layer weight footprint.
    size_t usable = 0;
    size_t overhead = headroom_bytes + info.non_layer_bytes;
    if (plan.vram_free_bytes > overhead)
        usable = plan.vram_free_bytes - overhead;

    // Greedily pack layers 0..N-1 until VRAM (weights + KV) is exhausted.
    // llama.cpp loads layers 0..n_gpu_layers-1 to the GPU.
    size_t cumulative = 0;
    int    n_gpu      = 0;
    for (int l = 0; l < info.n_layers; ++l) {
        size_t layer_sz = info.layer_bytes[static_cast<size_t>(l)];
        if (cumulative + layer_sz + kv_per_layer > usable) break;
        cumulative += layer_sz + kv_per_layer;
        ++n_gpu;
    }

    plan.n_gpu_layers        = n_gpu;
    plan.vram_layer_bytes    = cumulative;
    plan.vram_kv_cache_bytes = kv_per_layer * static_cast<size_t>(n_gpu);
    plan.vram_headroom_bytes = headroom_bytes;
    plan.fits_fully          = (n_gpu == info.n_layers);
    plan.needs_streaming     = (n_gpu < info.n_layers);

    for (int l = n_gpu; l < info.n_layers; ++l)
        plan.cpu_layer_bytes += info.layer_bytes[static_cast<size_t>(l)];

    plan.vram_after_bytes = plan.vram_free_bytes > plan.vram_layer_bytes + info.non_layer_bytes
        ? plan.vram_free_bytes - plan.vram_layer_bytes - info.non_layer_bytes
        : 0;

    return plan;
}

// ─────────────────────────────────────────────────────────────────────────────
// vram_plan_print
// ─────────────────────────────────────────────────────────────────────────────
static std::string human_bytes(size_t b)
{
    char buf[64];
    if      (b >= (1ULL << 30)) std::snprintf(buf, sizeof buf, "%.2f GiB", double(b) / (1 << 30));
    else if (b >= (1ULL << 20)) std::snprintf(buf, sizeof buf, "%.1f MiB", double(b) / (1 << 20));
    else                        std::snprintf(buf, sizeof buf, "%zu B", b);
    return buf;
}

void vram_plan_print(const VRAMPlan& plan, const std::string& model_path)
{
    std::cerr << "\n══ VRAM Plan";
    if (!model_path.empty()) std::cerr << " — " << model_path;
    std::cerr
        << " ══\n"
        << "  GPU layers : " << plan.n_gpu_layers
             << " / " << plan.n_total_layers
             << (plan.fits_fully ? "  [full offload]" : "  [partial offload]") << "\n"
        << "  VRAM free  : " << human_bytes(plan.vram_free_bytes) << "\n"
        << "  Layers     : " << human_bytes(plan.vram_layer_bytes)
             << "  (weights in GPU)\n"
        << "  KV cache   : " << human_bytes(plan.vram_kv_cache_bytes) << "\n"
        << "  Headroom   : " << human_bytes(plan.vram_headroom_bytes) << "\n"
        << "  Remaining  : " << human_bytes(plan.vram_after_bytes) << "\n"
        << "  CPU layers : " << human_bytes(plan.cpu_layer_bytes)
             << (plan.needs_streaming ? "  [streamed]" : "") << "\n"
        << "══\n\n";
}
