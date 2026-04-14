#include "SaveKvCacheEvent.hpp"

#include <llama.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

SaveKvCacheEvent::SaveKvCacheEvent(AIConvo& convo, std::filesystem::path output_dir)
    : _convo(convo)
    , _output_dir(std::move(output_dir))
{}

void SaveKvCacheEvent::Run() {
    llama_context* ctx = _convo.ConversationCtxPtr();
    if (!ctx)
        throw std::runtime_error("SaveKvCacheEvent: conversation context is null");

    std::filesystem::create_directories(_output_dir);
    const auto path = _output_dir / "kv_cache.bin";

    // Query the required buffer size, then copy the state bytes out.
    const size_t state_size = llama_state_get_size(ctx);
    std::vector<uint8_t> buffer(state_size);
    const size_t written = llama_state_get_data(ctx, buffer.data(), state_size);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        throw std::runtime_error("SaveKvCacheEvent: cannot open " + path.string());

    // Header: actual byte count written
    const uint64_t byte_count = static_cast<uint64_t>(written);
    out.write(reinterpret_cast<const char*>(&byte_count), sizeof(byte_count));

    // Payload: raw KV state bytes
    out.write(reinterpret_cast<const char*>(buffer.data()),
              static_cast<std::streamsize>(written));

    if (!out)
        throw std::runtime_error("SaveKvCacheEvent: write failed for " + path.string());
}
