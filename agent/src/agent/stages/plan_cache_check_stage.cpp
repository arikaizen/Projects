#include "plan_cache_check_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/plan_cache.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>

namespace agent {

static const char* CHECK_SCHEMA_STR = R"({
  "type": "object",
  "required": ["match"],
  "properties": {
    "match": {
      "type": "string",
      "enum": ["same", "changed", "different"],
      "description": "same=replay directly; changed=adapt affected steps; different=start fresh"
    },
    "reason":           {"type": "string"},
    "changed_aspects":  {"type": "array", "items": {"type": "string"}}
  }
})";

PlanCacheCheckStage::PlanCacheCheckStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "PlanCacheCheckStage", std::move(inputs)) {}

WorkResult PlanCacheCheckStage::execute(AgentContext& ctx) {
    auto start = std::chrono::steady_clock::now();
    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Stage";
    result.timestamp = std::chrono::system_clock::now();

    if (auto* bus = ctx.eventBus())
        bus->emit(EventBus::makeEvent("stage_start", {{"stage", name}, {"id", id}}));
    if (auto* logger = ctx.logger())
        logger->stageStart(ctx.config().agent_id, name, id, inputs);

    auto done = [&](bool success, const std::string& err = {}) {
        result.success  = success;
        result.error    = err;
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent(
                success ? "stage_done" : "stage_error",
                {{"stage", name}, {"id", id}, {"success", success}, {"error", err}}));
        }
    };

    try {
        std::string task = ctx.config().task;
        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        PlanCache* cache = ctx.planCache();

        // No cache wired — fall straight to UnderstandStage
        if (!cache) {
            std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") no cache backend — starting fresh\n";
            if (ctx.factory().isRegistered("UnderstandStage") && !ctx.idExists("auto_understand")) {
                auto s = ctx.factory().create("UnderstandStage", "auto_understand",
                                              nlohmann::json{{"task", task}});
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "fresh"}, {"reason", "no cache backend"}};
            done(true);
            return result;
        }

        auto cached = cache->load(ctx.config().agent_id);

        // No prior successful run — start fresh
        if (!cached) {
            std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") no cached plan — starting fresh\n";
            if (ctx.factory().isRegistered("UnderstandStage") && !ctx.idExists("auto_understand")) {
                auto s = ctx.factory().create("UnderstandStage", "auto_understand",
                                              nlohmann::json{{"task", task}});
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "fresh"}, {"reason", "no cached plan"}};
            done(true);
            return result;
        }

        // Exact task match — replay without any LLM call
        if (task == cached->task) {
            std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") exact task match — replaying\n";
            if (ctx.factory().isRegistered("ReplayStage") && !ctx.idExists("auto_replay")) {
                nlohmann::json ri;
                ri["steps"] = cached->steps;
                ri["task"]  = task;
                auto s = ctx.factory().create("ReplayStage", "auto_replay", ri);
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "replay"}, {"reason", "exact task match"},
                             {"run_count", cached->run_count}};
            done(true);
            return result;
        }

        // Task differs — ask LLM to classify the change
        std::string fingerprint_str = cached->fingerprint.is_null()
            ? "{}" : cached->fingerprint.dump(2);

        std::string system_prompt = ctx.promptLoader().render("plan_cache_check_stage", {
            {"TASK",               task},
            {"CACHED_TASK",        cached->task},
            {"CACHED_FINGERPRINT", fingerprint_str},
            {"OUTPUT_SCHEMA",      CHECK_SCHEMA_STR}
        });
        std::string user_msg = "Compare the current task against the cached run and classify.";

        std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") calling LLM for task diff\n";
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.1f, 512});
        if (!resp.success) {
            // LLM failed — default to fresh start, not an error
            std::cerr << "[STAGE] PlanCacheCheckStage LLM failed — defaulting to fresh start\n";
            if (ctx.factory().isRegistered("UnderstandStage") && !ctx.idExists("auto_understand")) {
                auto s = ctx.factory().create("UnderstandStage", "auto_understand",
                                              nlohmann::json{{"task", task}});
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "fresh"}, {"reason", "llm comparison failed"}};
            done(true);
            return result;
        }

        nlohmann::json assessment;
        try {
            assessment = nlohmann::json::parse(resp.content);
        } catch (...) {
            assessment = {{"match", "different"}, {"reason", "parse failure"}};
        }

        const std::string match          = assessment.value("match", std::string("different"));
        const std::string reason         = assessment.value("reason", std::string{});
        const nlohmann::json changed_asp = assessment.value("changed_aspects", nlohmann::json::array());

        std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") match=" << match << "\n";

        if (match == "same") {
            if (ctx.factory().isRegistered("ReplayStage") && !ctx.idExists("auto_replay")) {
                nlohmann::json ri;
                ri["steps"] = cached->steps;
                ri["task"]  = task;
                auto s = ctx.factory().create("ReplayStage", "auto_replay", ri);
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "replay"}, {"reason", reason}, {"run_count", cached->run_count}};
        } else if (match == "changed") {
            if (ctx.factory().isRegistered("PlanAdaptStage") && !ctx.idExists("auto_adapt")) {
                nlohmann::json ai;
                ai["cached_plan"]     = cached->steps;
                ai["changed_aspects"] = changed_asp;
                ai["task"]            = task;
                auto s = ctx.factory().create("PlanAdaptStage", "auto_adapt", ai);
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "adapt"}, {"reason", reason},
                             {"changed_aspects", changed_asp}};
        } else {
            // "different" — full fresh start
            if (ctx.factory().isRegistered("UnderstandStage") && !ctx.idExists("auto_understand")) {
                auto s = ctx.factory().create("UnderstandStage", "auto_understand",
                                              nlohmann::json{{"task", task}});
                ctx.push(std::move(s), AgentContext::Position::Back);
            }
            result.output = {{"route", "fresh"}, {"reason", reason}};
        }

        done(true);

    } catch (const std::exception& e) {
        std::cerr << "[STAGE] PlanCacheCheckStage(" << id << ") exception: " << e.what() << "\n";
        done(false, e.what());
    }

    return result;
}

std::string PlanCacheCheckStage::description() const {
    return "Cache check: replay unchanged plan, adapt changed plan, or start fresh";
}

void registerPlanCacheCheckStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "PlanCacheCheckStage",
            "Checks the plan cache and routes to ReplayStage, PlanAdaptStage, or UnderstandStage",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<PlanCacheCheckStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
