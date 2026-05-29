#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// ── BlackboardWriteAction ────────────────────────────────────────────────────
// Writes a key-value pair to the shared Blackboard (Pattern C).
// Thread-safe: Blackboard::write holds an internal mutex.
class BlackboardWriteAction : public Action {
public:
    BlackboardWriteAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// ── BlackboardReadAction ─────────────────────────────────────────────────────
// Reads a value from the shared Blackboard by key.
// Thread-safe: Blackboard::read holds an internal mutex.
class BlackboardReadAction : public Action {
public:
    BlackboardReadAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// ── BlackboardListAction ─────────────────────────────────────────────────────
// Lists all keys in the shared Blackboard, optionally filtered by prefix.
// Thread-safe: Blackboard::keys holds an internal mutex.
class BlackboardListAction : public Action {
public:
    BlackboardListAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// Registers BlackboardWriteAction, BlackboardReadAction, and BlackboardListAction.
void registerBlackboardActions(WorkFactory& factory);

} // namespace agent
