#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Adapts a cached plan to changed task parameters.
// Calls the LLM with the original cached steps and a description of what
// changed; receives a revised plan array; validates and pushes it with
// ObserveStage — same path as ReasonStage but seeded from cache knowledge.
class PlanAdaptStage : public Stage {
public:
    PlanAdaptStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerPlanAdaptStage(WorkFactory& factory);

} // namespace agent
