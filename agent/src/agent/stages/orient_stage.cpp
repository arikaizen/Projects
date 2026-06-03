#include "orient_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>

namespace agent {

OrientStage::OrientStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "OrientStage", std::move(inputs)) {}

WorkResult OrientStage::execute(AgentContext& ctx) {
    auto start = std::chrono::steady_clock::now();
    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Stage";
    result.timestamp = std::chrono::system_clock::now();

    if (auto* bus = ctx.eventBus()) {
        bus->emit(EventBus::makeEvent("stage_start", {{"stage", name}, {"id", id}}));
    }
    if (auto* logger = ctx.logger())
        logger->stageStart(ctx.config().agent_id, name, id, inputs);

    try {
        std::string task = ctx.config().task;
        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        std::string understanding_str = "{}";
        if (ctx.blackboard()) {
            auto val = ctx.blackboard()->read("agent:understanding");
            if (val) understanding_str = val->dump(2);
        }

        std::string catalog  = ctx.factory().toCatalogJson().dump(2);
        std::string history  = ctx.historySummaryJson(10).dump(2);

        std::string system_prompt = ctx.promptLoader().render("orient_stage", {
            {"TASK",          task},
            {"UNDERSTANDING", understanding_str},
            {"CATALOG",       catalog},
            {"HISTORY",       history}
        });
        std::string user_msg = "Orient yourself and return your situational assessment now.";

        std::cerr << "[STAGE] OrientStage(" << id << ") calling LLM\n";
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.2f, 1024});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        nlohmann::json orientation;
        try {
            orientation = nlohmann::json::parse(resp.content);
        } catch (...) {
            orientation = {{"raw", resp.content}};
        }

        if (ctx.blackboard())
            ctx.blackboard()->write("agent:orientation", orientation);
        if (auto* logger = ctx.logger())
            logger->blackboardWrite(ctx.config().agent_id, name, "agent:orientation", orientation);

        // Chain → LocateStage
        if (ctx.factory().isRegistered("LocateStage")) {
            std::string locate_id = "auto_locate";
            if (!ctx.idExists(locate_id)) {
                auto locate = ctx.factory().create("LocateStage", locate_id, {{"task", task}});
                ctx.push(std::move(locate), AgentContext::Position::Back);
            }
        }

        result.success = true;
        result.output  = orientation;
        std::cerr << "[STAGE] OrientStage(" << id << ") done\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] OrientStage(" << id << ") exception: " << e.what() << "\n";
        if (auto* bus = ctx.eventBus())
            bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    if (auto* logger = ctx.logger())
        logger->stageDone(ctx.config().agent_id, name, id, result.success,
                          result.output, result.duration.count(), result.error);
    if (auto* bus = ctx.eventBus()) {
        bus->emit(EventBus::makeEvent("stage_done", {
            {"stage", name}, {"id", id}, {"success", result.success}
        }));
    }
    return result;
}

std::string OrientStage::description() const {
    return "Step 2A — Orient: survey available tools, history, and context to build a situational picture";
}

void registerOrientStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "OrientStage",
            "Step 2A: surveys catalog, history, and blackboard to identify relevant tools and existing context",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<OrientStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
