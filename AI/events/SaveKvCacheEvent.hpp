#pragma once

#include "IEvent.hpp"
#include "convo.hpp"

#include <filesystem>
#include <string>

/**
 * SaveKvCacheEvent
 * ──────────────────────────────────────────────────────────────────────────
 * Responsibility: write kv_cache.bin
 *
 * Serialises the processed KV tensors from the conversation context using
 * llama_state_get_data().  Restoring this with llama_state_set_data() lets
 * the next session resume without re-decoding any history.
 *
 * Output path: <output_dir>/kv_cache.bin
 *
 * File format (binary):
 *   [uint64_t byte_count][raw bytes from llama_state_get_data]
 *
 * Requires AIConvo::ConversationCtxPtr() — see convo.hpp.
 */
class SaveKvCacheEvent : public IEvent {
public:
    SaveKvCacheEvent(AIConvo& convo, std::filesystem::path output_dir);

    void        Run()        override;
    std::string Name() const override { return "SaveKvCache"; }

private:
    AIConvo&              _convo;
    std::filesystem::path _output_dir;
};
