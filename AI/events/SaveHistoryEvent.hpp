#pragma once

#include "IEvent.hpp"
#include "convo.hpp"

#include <filesystem>
#include <string>

/**
 * SaveHistoryEvent
 * ──────────────────────────────────────────────────────────────────────────
 * Responsibility: write history.json
 *
 * Delegates directly to AIConvo::SaveHistory().
 * Output path: <output_dir>/history.json
 */
class SaveHistoryEvent : public IEvent {
public:
    SaveHistoryEvent(AIConvo& convo, std::filesystem::path output_dir);

    void        Run()        override;
    std::string Name() const override { return "SaveHistory"; }

private:
    AIConvo&              _convo;
    std::filesystem::path _output_dir;
};
