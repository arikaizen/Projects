#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Spawns a sub-agent to execute a task and waits for its result (Pattern A).
// Enforces max_depth from AgentConfig to prevent infinite recursion.
// Thread-safe: AgentManager methods are individually thread-safe.
class TaskAction : public Action {
public:
    TaskAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerTaskAction(WorkFactory& factory);

} // namespace agent
