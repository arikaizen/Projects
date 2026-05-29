#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"

namespace agent {

// LLM-powered text transformation.
// inputs: {"instruction": "...", "text": "..." or "$ref"}
class TransformStage : public Stage {
public:
    TransformStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerTransformStage(WorkFactory& factory);

} // namespace agent
