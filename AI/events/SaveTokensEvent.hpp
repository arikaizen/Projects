#pragma once

#include "IEvent.hpp"
#include "convo.hpp"

#include <filesystem>
#include <string>

/**
 * SaveTokensEvent
 * ──────────────────────────────────────────────────────────────────────────
 * Responsibility: write tokens.bin
 *
 * Serialises the current conversation as a flat array of int32_t token IDs.
 * Loading this back lets the next session skip re-tokenising the history.
 *
 * Output path: <output_dir>/tokens.bin
 *
 * File format (binary, little-endian):
 *   [uint64_t count][int32_t token_0][int32_t token_1]...[int32_t token_N-1]
 *
 * Requires AIConvo::GetCurrentTokens() — see convo.hpp.
 */
class SaveTokensEvent : public IEvent {
public:
    SaveTokensEvent(AIConvo& convo, std::filesystem::path output_dir);

    void        Run()        override;
    std::string Name() const override { return "SaveTokens"; }

private:
    AIConvo&              _convo;
    std::filesystem::path _output_dir;
};
