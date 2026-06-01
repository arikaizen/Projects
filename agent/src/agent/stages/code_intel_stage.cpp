#include "code_intel_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include <chrono>
#include <iostream>

namespace agent {

CodeIntelStage::CodeIntelStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "CodeIntelStage", std::move(inputs)) {}

WorkResult CodeIntelStage::execute(AgentContext& ctx) {
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

        std::string read_ctx_str = "{}";
        if (ctx.blackboard()) {
            if (auto v = ctx.blackboard()->read("agent:read_context")) read_ctx_str = v->dump(2);
        }

        // Include the last N history items (file reads, grep results, etc.)
        std::string history = ctx.historySummaryJson(15).dump(2);

        std::string system_prompt = ctx.promptLoader().render("code_intel_stage", {
            {"TASK",         task},
            {"READ_CONTEXT", read_ctx_str},
            {"HISTORY",      history}
        });
        std::string user_msg = "Analyse the code structure and return your intelligence report now.";

        std::cerr << "[STAGE] CodeIntelStage(" << id << ") calling LLM\n";
        auto resp = ctx.llm().complete({system_prompt, user_msg, /*json_mode=*/true, 0.2f, 2048});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        nlohmann::json intel;
        try {
            intel = nlohmann::json::parse(resp.content);
        } catch (...) {
            intel = {{"raw", resp.content}};
        }

        if (ctx.blackboard())
            ctx.blackboard()->write("agent:code_intel", intel);

        // Chain → ReasonStage
        if (ctx.factory().isRegistered("ReasonStage")) {
            std::string reason_id = "auto_reason";
            if (!ctx.idExists(reason_id)) {
                auto reason = ctx.factory().create("ReasonStage", reason_id, {{"task", task}});
                ctx.push(std::move(reason), AgentContext::Position::Back);
            }
        }

        result.success = true;
        result.output  = intel;
        std::cerr << "[STAGE] CodeIntelStage(" << id << ") done\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] CodeIntelStage(" << id << ") exception: " << e.what() << "\n";
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

std::string CodeIntelStage::description() const {
    return "Step 2E — Code intelligence: analyse code structures, patterns, and dependencies from gathered content";
}

void registerCodeIntelStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "CodeIntelStage",
            "Step 2E (optional): analyses code structure, types, and call relationships from prior read results",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<CodeIntelStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
