#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <string>

namespace agent {

// Step 2B — Locate.
// Asks the LLM to identify which specific resources (files, symbols, memory
// entries) need to be examined.  Pushes the locate actions (GlobAction,
// GrepAction, MemoryReadAction, etc.) that it decides on, then pushes a
// ReadStage that declares $ref dependencies on all of them so it runs only
// after every locate action has completed.
class LocateStage : public Stage {
public:
    LocateStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;
};

void registerLocateStage(WorkFactory& factory);

} // namespace agent
