#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Recursively searches a directory tree for paths matching a glob pattern.
// Supports * (any sequence of chars within a path component) and ?
// (any single char).  Thread-safe: only reads the filesystem.
class GlobAction : public Action {
public:
    GlobAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;

private:
    // Returns true if `name` matches shell-style wildcard `pattern`.
    // Handles * and ? metacharacters; no character classes.
    static bool wildcardMatch(const std::string& pattern, const std::string& name);
};

void registerGlobAction(WorkFactory& factory);

} // namespace agent
