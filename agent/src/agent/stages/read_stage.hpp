#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 2C — Read.
// Receives the results of locate actions via $ref-resolved inputs, synthesises
// the gathered context, and writes a summary to the blackboard under
// "agent:read_context".  Chains to CodeIntelStage (when inputs["code_intel"]
// is true) or directly to ReasonStage.
class ReadStage : public Stage {
public:
    ReadStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerReadStage(WorkFactory& factory);

} // namespace agent
