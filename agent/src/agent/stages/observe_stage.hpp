#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 5 — Observe.
// Pushed automatically by ReasonStage after every plan, with $ref
// dependencies on all plan items so it runs only once they are all complete.
// Inspects success/failure of the executed plan, decides whether the overall
// task is done, and either chains to RespondStage (done) or pushes a new
// ReasonStage (another iteration needed).
class ObserveStage : public Stage {
public:
    ObserveStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerObserveStage(WorkFactory& factory);

} // namespace agent
