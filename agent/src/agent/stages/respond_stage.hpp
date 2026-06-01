#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 6 — Respond.
// Composes the final answer to the user from the full execution history and
// any blackboard summaries.  Sets ctx.final_output and ctx.should_stop = true
// to terminate the agent loop.
class RespondStage : public Stage {
public:
    RespondStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerRespondStage(WorkFactory& factory);

} // namespace agent
