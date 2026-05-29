#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Stub web search action.
// A real implementation would call a search API (e.g. SerpAPI, Brave Search)
// using a key stored in AgentContext config or environment variables.
// As shipped, this returns a placeholder response indicating API key setup is needed.
// Thread-safe: no shared mutable state.
class WebSearchAction : public Action {
public:
    WebSearchAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerWebSearchAction(WorkFactory& factory);

} // namespace agent
