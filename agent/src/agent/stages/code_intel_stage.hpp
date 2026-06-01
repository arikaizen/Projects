#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 2E — Code intelligence (optional).
// Analyses code structure from prior ReadAction / GrepAction results in
// history: identifies types, call relationships, patterns, and entry points.
// Writes its findings to the blackboard under "agent:code_intel" and chains
// to ReasonStage.  Pushed by ReadStage when inputs["code_intel"] is true,
// or included directly in a ReasonStage plan.
class CodeIntelStage : public Stage {
public:
    CodeIntelStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerCodeIntelStage(WorkFactory& factory);

} // namespace agent
