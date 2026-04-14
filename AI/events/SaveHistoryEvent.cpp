#include "SaveHistoryEvent.hpp"

#include <filesystem>
#include <stdexcept>

SaveHistoryEvent::SaveHistoryEvent(AIConvo& convo, std::filesystem::path output_dir)
    : _convo(convo)
    , _output_dir(std::move(output_dir))
{}

void SaveHistoryEvent::Run() {
    std::filesystem::create_directories(_output_dir);
    const auto path = (_output_dir / "history.json").string();
    _convo.SaveHistory(path);
}
