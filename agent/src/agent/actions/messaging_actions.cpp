#include "messaging_actions.hpp"
#include "agent/agent_context.hpp"
#include "agent/agent_manager.hpp"
#include "agent/message_inbox.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

// ── SendMessageAction ────────────────────────────────────────────────────────

WorkResult SendMessageAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto to       = resolved.at("to").get<std::string>();
        auto message  = resolved.at("message"); // any JSON value

        std::cerr << "[ACTION:" << name << "] sending message to=\"" << to << "\"\n";

        if (ctx.manager() == nullptr) {
            throw std::runtime_error("SendMessageAction requires an AgentManager");
        }

        ctx.manager()->sendMessage(ctx.config().agent_id, to, message);

        result.success = true;
        result.output  = {{"sent", true}, {"to", to}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── ReceiveMessagesAction ────────────────────────────────────────────────────

WorkResult ReceiveMessagesAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        std::cerr << "[ACTION:" << name << "] draining inbox for agent \""
                  << ctx.config().agent_id << "\"\n";

        if (ctx.manager() == nullptr) {
            throw std::runtime_error("ReceiveMessagesAction requires an AgentManager");
        }

        auto msgs = ctx.manager()->drainInbox(ctx.config().agent_id);

        nlohmann::json msgs_json = nlohmann::json::array();
        for (const auto& m : msgs) {
            msgs_json.push_back({
                {"from_id",   m.from_id},
                {"to_id",     m.to_id},
                {"payload",   m.payload},
                {"timestamp", m.timestamp}
            });
        }

        result.success = true;
        result.output  = {{"messages", msgs_json}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── Registration ─────────────────────────────────────────────────────────────

void registerMessagingActions(WorkFactory& factory) {
    // SendMessageAction
    factory.registerItem(
        WorkItemSpec{
            "SendMessageAction",
            "Send a JSON message to another agent's inbox (Pattern B messaging).",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"required", {"to", "message"}},
                {"properties", {
                    {"to",      {{"type", "string"}, {"description", "Destination agent ID."}}},
                    {"message", {{"description", "Arbitrary JSON payload to send."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<SendMessageAction>(std::move(id), "SendMessageAction", std::move(inputs));
        }
    );

    // ReceiveMessagesAction
    factory.registerItem(
        WorkItemSpec{
            "ReceiveMessagesAction",
            "Drain this agent's inbox and return all pending messages.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"properties", nlohmann::json::object()}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<ReceiveMessagesAction>(std::move(id), "ReceiveMessagesAction", std::move(inputs));
        }
    );
}

} // namespace agent
