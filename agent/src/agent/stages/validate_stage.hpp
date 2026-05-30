#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"

namespace agent {

// LLM-powered validation of a previous result.
// inputs: {"target_id": "...", "criteria": "...", "corrective_injection": bool}
class ValidateStage : public Stage {
public:
    ValidateStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerValidateStage(WorkFactory& factory);

} // namespace agent
