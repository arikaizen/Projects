#include "todo_write_action.hpp"
#include "agent/agent_context.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult TodoWriteAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved  = ctx.resolveReferences(inputs);
        auto operation = resolved.at("operation").get<std::string>();

        std::cerr << "[ACTION:" << name << "] todo operation: " << operation << "\n";

        if (operation == "add") {
            auto item = resolved.at("item").get<std::string>();
            ctx.todo_list.push_back(item);
            result.success = true;
            result.output  = {{"todo_list", ctx.todo_list}};

        } else if (operation == "remove") {
            auto item = resolved.at("item").get<std::string>();
            auto it   = std::find(ctx.todo_list.begin(), ctx.todo_list.end(), item);
            if (it != ctx.todo_list.end()) {
                ctx.todo_list.erase(it);
            }
            result.success = true;
            result.output  = {{"todo_list", ctx.todo_list}};

        } else if (operation == "clear") {
            ctx.todo_list.clear();
            result.success = true;
            result.output  = {{"todo_list", nlohmann::json::array()}};

        } else if (operation == "list") {
            result.success = true;
            result.output  = {{"todo_list", ctx.todo_list}};

        } else {
            throw std::runtime_error("Unknown operation: " + operation +
                                     ". Valid: add, remove, clear, list.");
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerTodoWriteAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "TodoWriteAction",
        "Manage the agent's todo list. Operations: add, remove, clear, list.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"operation"}},
            {"properties", {
                {"operation", {{"type", "string"}, {"enum", {"add", "remove", "clear", "list"}},
                               {"description", "Operation to perform."}}},
                {"item",      {{"type", "string"}, {"description", "Todo item text (required for add/remove)."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<TodoWriteAction>(std::move(id), "TodoWriteAction", std::move(inputs));
    });
}

} // namespace agent
