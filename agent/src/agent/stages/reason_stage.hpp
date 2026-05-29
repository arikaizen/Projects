#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Primary reasoning stage (Reading 1).
// Renders reason_stage.md, calls the LLM to produce a plan,
// validates it, and pushes the plan items onto the queue.
class ReasonStage : public Stage {
public:
    ReasonStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;

private:
    bool validateAndPushPlan(const nlohmann::json& plan, AgentContext& ctx,
                              std::string& error_out);
};

void registerReasonStage(WorkFactory& factory);

} // namespace agent
