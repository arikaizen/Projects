#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 1 — Understand the goal.
// Analyses the raw task string and extracts a structured goal (objective,
// constraints, expected output type, domain).  Writes the result to the
// blackboard under "agent:understanding" and chains to OrientStage.
class UnderstandStage : public Stage {
public:
    UnderstandStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerUnderstandStage(WorkFactory& factory);

} // namespace agent
