#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Searches file contents for a pattern.
// Tries to exec ripgrep (rg) first for performance; falls back to a manual
// std::ifstream search if rg is not available.
// Thread-safe: read-only filesystem access.
class GrepAction : public Action {
public:
    GrepAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;

private:
    // Returns true if `rg` is available on PATH.
    static bool ripgrepAvailable();

    // Run rg and collect results.
    nlohmann::json runRipgrep(const std::string& pattern,
                               const std::string& path,
                               bool               use_regex);

    // Manual fallback: walks files and uses std::string::find or std::regex.
    nlohmann::json manualGrep(const std::string& pattern,
                               const std::string& path,
                               bool               use_regex);
};

void registerGrepAction(WorkFactory& factory);

} // namespace agent
