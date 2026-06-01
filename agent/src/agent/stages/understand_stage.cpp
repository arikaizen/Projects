#include "understand_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include <chrono>
#include <iostream>

namespace agent {

UnderstandStage::UnderstandStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "UnderstandStage", std::move(inputs)) {}

WorkResult UnderstandStage::execute(AgentContext& ctx) {
    auto start = std::chrono::steady_clock::now();
    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Stage";
    result.timestamp = std::chrono::system_clock::now();

    if (auto* bus = ctx.eventBus()) {
        bus->emit(EventBus::makeEvent("stage_start", {{"stage", name}, {"id", id}}));
    }

    try {
        std::string task = ctx.config().task;
        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        std::string system_prompt = ctx.promptLoader().render("understand_stage", {
            {"TASK", task}
        });
        std::string user_msg = "Analyse the task and return your structured understanding now.";

        std::cerr << "[STAGE] UnderstandStage(" << id << ") calling LLM\n";
        auto resp = ctx.llm().complete({system_prompt, user_msg, /*json_mode=*/true, 0.1f, 1024});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        nlohmann::json understanding;
        try {
            understanding = nlohmann::json::parse(resp.content);
        } catch (...) {
            understanding = {{"raw", resp.content}};
        }

        if (ctx.blackboard())
            ctx.blackboard()->write("agent:understanding", understanding);

        // Chain → OrientStage
        if (ctx.factory().isRegistered("OrientStage")) {
            std::string orient_id = "auto_orient";
            if (!ctx.idExists(orient_id)) {
                auto orient = ctx.factory().create("OrientStage", orient_id, {{"task", task}});
                ctx.push(std::move(orient), AgentContext::Position::Back);
            }
        }

        result.success = true;
        result.output  = understanding;
        std::cerr << "[STAGE] UnderstandStage(" << id << ") done\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] UnderstandStage(" << id << ") exception: " << e.what() << "\n";
        if (auto* bus = ctx.eventBus())
            bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    if (auto* bus = ctx.eventBus()) {
        bus->emit(EventBus::makeEvent("stage_done", {
            {"stage", name}, {"id", id}, {"success", result.success}
        }));
    }
    return result;
}

std::string UnderstandStage::description() const {
    return "Step 1 — Understand the goal: parse task into objective, constraints, output type, domain";
}

void registerUnderstandStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "UnderstandStage",
            "Step 1: parses the task into structured goal, constraints, output type, and domain",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<UnderstandStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
