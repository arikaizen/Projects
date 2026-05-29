#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"

namespace agent {

// Meta-stage (Reading 2): operates on the previous item's output and decides
// what to inject next.  Distinct from ReasonStage — its prompt focuses on
// "given this output, what next?" rather than "given the whole state, what next?".
class InjectionStage : public Stage {
public:
    InjectionStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;

private:
    bool validateAndPushPlan(const nlohmann::json& plan, AgentContext& ctx,
                              std::string& error_out);
};

void registerInjectionStage(WorkFactory& factory);

} // namespace agent
