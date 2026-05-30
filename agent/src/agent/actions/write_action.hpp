#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Writes content to a file, creating parent directories as needed.
// Concurrent writes to the SAME path are inherently unsafe — callers must
// ensure only one WriteAction targets a given path at a time.
class WriteAction : public Action {
public:
    WriteAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerWriteAction(WorkFactory& factory);

} // namespace agent
