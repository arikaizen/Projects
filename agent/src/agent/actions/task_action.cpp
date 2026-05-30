#include "task_action.hpp"
#include "agent/agent_context.hpp"
#include "agent/agent_manager.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult TaskAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved   = ctx.resolveReferences(inputs);
        auto task       = resolved.at("task").get<std::string>();
        auto agent_name = resolved.value("agent_name", std::string("sub-agent"));

        std::cerr << "[ACTION:" << name << "] spawning sub-agent \"" << agent_name
                  << "\" for task: " << task << "\n";

        if (ctx.manager() == nullptr) {
            throw std::runtime_error("TaskAction requires an AgentManager (ctx.manager() is null)");
        }

        const auto& cfg = ctx.config();
        if (cfg.current_depth >= cfg.max_depth) {
            throw std::runtime_error(
                "Max agent depth exceeded (" + std::to_string(cfg.max_depth) +
                "). Cannot spawn sub-agent.");
        }

        // Build sub-agent config.
        AgentConfig sub_config;
        sub_config.agent_id       = "";  // manager assigns
        sub_config.name           = agent_name;
        sub_config.task           = task;
        sub_config.max_iterations = cfg.max_iterations;
        sub_config.max_depth      = cfg.max_depth;
        sub_config.current_depth  = cfg.current_depth + 1;

        std::string sub_id = ctx.manager()->spawnAgent(sub_config);

        nlohmann::json sub_result = ctx.manager()->runAgentBlocking(sub_id, task);

        ctx.manager()->destroyAgent(sub_id);

        result.success = true;
        result.output  = sub_result;
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerTaskAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "TaskAction",
        "Spawn a sub-agent to perform a task and return its result (Pattern A delegation).",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"task"}},
            {"properties", {
                {"task",       {{"type", "string"}, {"description", "Task description for the sub-agent."}}},
                {"agent_name", {{"type", "string"}, {"description", "Optional display name for the sub-agent."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<TaskAction>(std::move(id), "TaskAction", std::move(inputs));
    });
}

} // namespace agent
