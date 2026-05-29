#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Performs an exact first-occurrence string replacement in a file.
// Concurrent edits to the SAME file are unsafe — callers must serialize.
class EditAction : public Action {
public:
    EditAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerEditAction(WorkFactory& factory);

} // namespace agent
