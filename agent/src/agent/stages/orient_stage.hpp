#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 2A — Orient.
// Surveys the available tool catalog, existing history, and blackboard state
// to build a situational picture.  Writes the orientation to the blackboard
// under "agent:orientation" and chains to LocateStage.
class OrientStage : public Stage {
public:
    OrientStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerOrientStage(WorkFactory& factory);

} // namespace agent
