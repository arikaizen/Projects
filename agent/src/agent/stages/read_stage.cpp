#include "read_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include <chrono>
#include <iostream>

namespace agent {

ReadStage::ReadStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "ReadStage", std::move(inputs)) {}

WorkResult ReadStage::execute(AgentContext& ctx) {
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

        // inputs["locate_results"] has been $ref-resolved by BatchExecutor:
        // it is now an object mapping action_id -> action output.
        std::string locate_results_str = "{}";
        if (inputs.contains("locate_results"))
            locate_results_str = inputs["locate_results"].dump(2);

        std::string understanding_str = "{}";
        std::string orientation_str   = "{}";
        if (ctx.blackboard()) {
            if (auto v = ctx.blackboard()->read("agent:understanding")) understanding_str = v->dump(2);
            if (auto v = ctx.blackboard()->read("agent:orientation"))   orientation_str   = v->dump(2);
        }

        std::string system_prompt = ctx.promptLoader().render("read_stage", {
            {"TASK",           task},
            {"UNDERSTANDING",  understanding_str},
            {"ORIENTATION",    orientation_str},
            {"LOCATE_RESULTS", locate_results_str}
        });
        std::string user_msg = "Synthesise the gathered context and return your findings now.";

        std::cerr << "[STAGE] ReadStage(" << id << ") calling LLM\n";
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

        nlohmann::json read_ctx;
        try {
            read_ctx = nlohmann::json::parse(resp.content);
        } catch (...) {
            read_ctx = {{"raw", resp.content}};
        }

        if (ctx.blackboard())
            ctx.blackboard()->write("agent:read_context", read_ctx);

        // Chain → CodeIntelStage (if requested) or ReasonStage
        const bool use_code_intel = inputs.value("code_intel", false) ||
            (read_ctx.contains("needs_code_intel") && read_ctx["needs_code_intel"].is_boolean() &&
             read_ctx["needs_code_intel"].get<bool>());

        if (use_code_intel && ctx.factory().isRegistered("CodeIntelStage")) {
            std::string ci_id = "auto_code_intel";
            if (!ctx.idExists(ci_id)) {
                auto ci = ctx.factory().create("CodeIntelStage", ci_id, {{"task", task}});
                ctx.push(std::move(ci), AgentContext::Position::Back);
            }
        } else if (ctx.factory().isRegistered("ReasonStage")) {
            std::string reason_id = "auto_reason";
            if (!ctx.idExists(reason_id)) {
                auto reason = ctx.factory().create("ReasonStage", reason_id, {{"task", task}});
                ctx.push(std::move(reason), AgentContext::Position::Back);
            }
        }

        result.success = true;
        result.output  = read_ctx;
        std::cerr << "[STAGE] ReadStage(" << id << ") done\n";

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] ReadStage(" << id << ") exception: " << e.what() << "\n";
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

std::string ReadStage::description() const {
    return "Step 2C — Read: synthesise locate results into structured context, chain to Reason";
}

void registerReadStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "ReadStage",
            "Step 2C: synthesises locate-action results into a context summary; chains to CodeIntelStage or ReasonStage",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"properties", {
                 {"task",           {{"type", "string"}}},
                 {"locate_results", {{"type", "object"}, {"description", "$ref-resolved locate action outputs"}}},
                 {"code_intel",     {{"type", "boolean"}, {"description", "Force code intelligence pass before reasoning"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<ReadStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
