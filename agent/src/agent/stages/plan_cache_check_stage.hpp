#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Runs before UnderstandStage when a plan cache exists for this agent.
// If the task is identical to the cached run, routes to ReplayStage (no LLM).
// If the task changed, calls the LLM to classify the change and routes to
// PlanAdaptStage (partial change) or UnderstandStage (completely different).
class PlanCacheCheckStage : public Stage {
public:
    PlanCacheCheckStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerPlanCacheCheckStage(WorkFactory& factory);

} // namespace agent
