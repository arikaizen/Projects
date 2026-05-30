#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Reads a file from disk, optionally selecting a range of lines.
// offset is 1-based; limit is the maximum number of lines to return.
// Concurrent reads of the same file are safe (no writes performed).
class ReadAction : public Action {
public:
    ReadAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerReadAction(WorkFactory& factory);

} // namespace agent
