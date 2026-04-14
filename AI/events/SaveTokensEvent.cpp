#include "SaveTokensEvent.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

SaveTokensEvent::SaveTokensEvent(AIConvo& convo, std::filesystem::path output_dir)
    : _convo(convo)
    , _output_dir(std::move(output_dir))
{}

void SaveTokensEvent::Run() {
    std::filesystem::create_directories(_output_dir);
    const auto path = _output_dir / "tokens.bin";

    const auto tokens = _convo.GetCurrentTokens();

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        throw std::runtime_error("SaveTokensEvent: cannot open " + path.string());

    // Header: number of tokens as uint64_t
    const uint64_t count = static_cast<uint64_t>(tokens.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Payload: raw int32_t token IDs
    if (!tokens.empty())
        out.write(reinterpret_cast<const char*>(tokens.data()),
                  static_cast<std::streamsize>(tokens.size() * sizeof(int32_t)));

    if (!out)
        throw std::runtime_error("SaveTokensEvent: write failed for " + path.string());
}
