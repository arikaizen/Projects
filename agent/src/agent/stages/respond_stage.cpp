#include "respond_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>

namespace agent {

RespondStage::RespondStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "RespondStage", std::move(inputs)) {}

WorkResult RespondStage::execute(AgentContext& ctx) {
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
        std::string task    = ctx.config().task;
        std::string history = ctx.historySummaryJson(30).dump(2);

        std::string read_ctx_str  = "{}";
        std::string code_intel_str = "{}";
        if (ctx.blackboard()) {
            if (auto v = ctx.blackboard()->read("agent:read_context")) read_ctx_str  = v->dump(2);
            if (auto v = ctx.blackboard()->read("agent:code_intel"))   code_intel_str = v->dump(2);
        }

        std::string system_prompt = ctx.promptLoader().render("respond_stage", {
            {"TASK",        task},
            {"HISTORY",     history},
            {"READ_CONTEXT", read_ctx_str},
            {"CODE_INTEL",  code_intel_str}
        });
        std::string user_msg = "Compose the final response to the user now.";

        std::cerr << "[STAGE] RespondStage(" << id << ") calling LLM\n";
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.3f, 4096});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        nlohmann::json response_json;
        try {
            response_json = nlohmann::json::parse(resp.content);
        } catch (...) {
            response_json = {{"answer", resp.content}};
        }

        const std::string answer = response_json.value("answer", resp.content);

        ctx.should_stop  = true;
        ctx.final_output = {{"answer", answer}};
        if (auto* logger = ctx.logger())
            logger->finalAnswer(ctx.config().agent_id, ctx.final_output, ctx.iteration_count);

        result.success = true;
        result.output  = ctx.final_output;

        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent("agent_final_answer",
                {{"stage", name}, {"answer", answer}}));
        }
        std::cerr << "[STAGE] RespondStage(" << id << ") agent complete\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] RespondStage(" << id << ") exception: " << e.what() << "\n";
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

std::string RespondStage::description() const {
    return "Step 6 — Respond: compose the final answer from history and blackboard, terminate the agent";
}

void registerRespondStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "RespondStage",
            "Step 6: composes the final user-facing answer from execution history; sets should_stop",
            WorkItem::Kind::Stage,
            {{"type", "object"}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<RespondStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
