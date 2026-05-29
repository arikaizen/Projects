#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Runs a shell command via popen() and captures stdout + exit code.
// Thread-safety note: popen() is not strictly thread-safe in all libc
// implementations.  Concurrent BashAction executions are safe in glibc on
// Linux; on other platforms serialize with an external mutex if needed.
class BashAction : public Action {
public:
    BashAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerBashAction(WorkFactory& factory);

} // namespace agent
