#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Manages the per-agent todo list stored in AgentContext::todo_list.
// Operations: add, remove, clear, list.
// Thread-safety note: todo_list is not independently mutex-guarded — all
// WorkItems execute on the agent's single event loop, so no concurrent access
// occurs in the default single-threaded execution model.
class TodoWriteAction : public Action {
public:
    TodoWriteAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

void registerTodoWriteAction(WorkFactory& factory);

} // namespace agent
