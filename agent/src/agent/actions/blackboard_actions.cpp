#include "blackboard_actions.hpp"
#include "agent/agent_context.hpp"
#include "agent/blackboard.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

// ── BlackboardWriteAction ────────────────────────────────────────────────────

WorkResult BlackboardWriteAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto key      = resolved.at("key").get<std::string>();
        auto value    = resolved.at("value"); // arbitrary JSON

        std::cerr << "[ACTION:" << name << "] blackboard write key=\"" << key << "\"\n";

        if (ctx.blackboard() == nullptr) {
            throw std::runtime_error("BlackboardWriteAction: no Blackboard attached to context");
        }

        ctx.blackboard()->write(key, value);

        result.success = true;
        result.output  = {{"key", key}, {"written", true}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── BlackboardReadAction ─────────────────────────────────────────────────────

WorkResult BlackboardReadAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto key      = resolved.at("key").get<std::string>();

        std::cerr << "[ACTION:" << name << "] blackboard read key=\"" << key << "\"\n";

        if (ctx.blackboard() == nullptr) {
            throw std::runtime_error("BlackboardReadAction: no Blackboard attached to context");
        }

        auto opt_value = ctx.blackboard()->read(key);
        if (opt_value.has_value()) {
            result.success = true;
            result.output  = {{"key", key}, {"value", *opt_value}, {"found", true}};
        } else {
            result.success = false;
            result.error   = "Key not found: " + key;
            result.output  = {{"key", key}, {"found", false}};
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── BlackboardListAction ─────────────────────────────────────────────────────

WorkResult BlackboardListAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto prefix   = resolved.value("prefix", std::string(""));

        std::cerr << "[ACTION:" << name << "] blackboard list prefix=\"" << prefix << "\"\n";

        if (ctx.blackboard() == nullptr) {
            throw std::runtime_error("BlackboardListAction: no Blackboard attached to context");
        }

        auto ks = ctx.blackboard()->keys(prefix);

        nlohmann::json keys_json = nlohmann::json::array();
        for (const auto& k : ks) {
            keys_json.push_back(k);
        }

        result.success = true;
        result.output  = {{"keys", keys_json}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── Registration ─────────────────────────────────────────────────────────────

void registerBlackboardActions(WorkFactory& factory) {
    // BlackboardWriteAction
    factory.registerItem(
        WorkItemSpec{
            "BlackboardWriteAction",
            "Write a key-value pair to the shared Blackboard (Pattern C).",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"required", {"key", "value"}},
                {"properties", {
                    {"key",   {{"type", "string"}, {"description", "Blackboard key."}}},
                    {"value", {{"description", "Any JSON value to store."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<BlackboardWriteAction>(std::move(id), "BlackboardWriteAction", std::move(inputs));
        }
    );

    // BlackboardReadAction
    factory.registerItem(
        WorkItemSpec{
            "BlackboardReadAction",
            "Read a value from the shared Blackboard by key.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"required", {"key"}},
                {"properties", {
                    {"key", {{"type", "string"}, {"description", "Blackboard key to read."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<BlackboardReadAction>(std::move(id), "BlackboardReadAction", std::move(inputs));
        }
    );

    // BlackboardListAction
    factory.registerItem(
        WorkItemSpec{
            "BlackboardListAction",
            "List all Blackboard keys, optionally filtered by prefix.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"properties", {
                    {"prefix", {{"type", "string"}, {"description", "Optional key prefix filter."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<BlackboardListAction>(std::move(id), "BlackboardListAction", std::move(inputs));
        }
    );
}

} // namespace agent
