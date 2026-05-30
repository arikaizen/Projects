#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// ── SendMessageAction ────────────────────────────────────────────────────────
// Sends a JSON message to another agent's inbox via AgentManager (Pattern B).
// Thread-safe: AgentManager::sendMessage is thread-safe.
class SendMessageAction : public Action {
public:
    SendMessageAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// ── ReceiveMessagesAction ────────────────────────────────────────────────────
// Drains this agent's inbox and returns all pending messages.
// Thread-safe: AgentManager::drainInbox is thread-safe.
class ReceiveMessagesAction : public Action {
public:
    ReceiveMessagesAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// Registers SendMessageAction and ReceiveMessagesAction.
void registerMessagingActions(WorkFactory& factory);

} // namespace agent
