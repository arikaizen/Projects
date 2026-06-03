#include "observe_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/plan_cache.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace agent {

static const char* OBSERVE_SCHEMA_STR = R"({
  "type": "object",
  "required": ["done", "next_action"],
  "properties": {
    "done":         {"type": "boolean", "description": "true when the overall task is complete"},
    "observations": {"type": "array",  "items": {"type": "string"}},
    "failures":     {"type": "array",  "items": {"type": "string"}},
    "next_action":  {"type": "string", "enum": ["respond", "iterate"],
                     "description": "respond = task done, chain to RespondStage; iterate = push another ReasonStage"},
    "next_task":    {"type": "string", "description": "Refined task description for the next reasoning iteration"}
  }
})";

ObserveStage::ObserveStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "ObserveStage", std::move(inputs)) {}

WorkResult ObserveStage::execute(AgentContext& ctx) {
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

        // inputs["plan_results"] is the $ref-resolved array of plan item outputs
        // (injected by ReasonStage in validateAndPushPlan).
        std::string plan_results_str = "[]";
        if (inputs.contains("plan_results"))
            plan_results_str = inputs["plan_results"].dump(2);

        std::string history = ctx.historySummaryJson(20).dump(2);

        std::string system_prompt = ctx.promptLoader().render("observe_stage", {
            {"TASK",          task},
            {"PLAN_RESULTS",  plan_results_str},
            {"HISTORY",       history},
            {"OUTPUT_SCHEMA", OBSERVE_SCHEMA_STR}
        });
        std::string user_msg = "Observe the execution results and return your assessment now.";

        std::cerr << "[STAGE] ObserveStage(" << id << ") calling LLM\n";
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

        nlohmann::json observation;
        try {
            observation = nlohmann::json::parse(resp.content);
        } catch (const std::exception& ex) {
            result.success = false;
            result.error   = std::string("LLM returned invalid JSON: ") + ex.what();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        const bool        done        = observation.value("done", false);
        const std::string next_action = observation.value("next_action", std::string("respond"));
        const std::string next_task   = observation.value("next_task", task);

        std::cerr << "[STAGE] ObserveStage(" << id << ") done=" << done
                  << " next_action=" << next_action << "\n";

        if (auto* logger = ctx.logger())
            logger->observeDecision(ctx.config().agent_id, id, done, next_action,
                                    observation.value("observations", nlohmann::json::array()),
                                    observation.value("failures", nlohmann::json::array()));

        if (done || next_action == "respond") {
            // Persist the successful plan to PlanCache for future replay/adaptation
            if (auto* cache = ctx.planCache()) {
                auto* bb = ctx.blackboard();
                nlohmann::json last_plan;
                nlohmann::json fingerprint;
                if (bb) {
                    if (auto v = bb->read("agent:last_plan"))    last_plan   = *v;
                    if (auto v = bb->read("agent:understanding")) fingerprint = *v;
                }
                if (last_plan.is_array() && !last_plan.empty()) {
                    // Load existing entry to increment run_count on a replay
                    auto existing = cache->load(ctx.config().agent_id);
                    CachedPlan cp;
                    cp.task        = ctx.config().task;
                    cp.fingerprint = fingerprint;
                    cp.steps       = last_plan;
                    cp.run_count   = existing ? existing->run_count + 1 : 0;
                    // ISO-8601 UTC timestamp
                    auto tp  = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(tp);
                    std::tm tm_utc{};
#if defined(_WIN32)
                    gmtime_s(&tm_utc, &t);
#else
                    gmtime_r(&t, &tm_utc);
#endif
                    std::ostringstream oss;
                    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
                    cp.created_at = oss.str();
                    try { cache->save(ctx.config().agent_id, cp); }
                    catch (const std::exception& ex) {
                        std::cerr << "[STAGE] ObserveStage: cache save failed: " << ex.what() << "\n";
                    }
                    std::cerr << "[STAGE] ObserveStage(" << id << ") saved plan to cache (run_count="
                              << cp.run_count << ")\n";
                }
            }

            // Task complete — chain to RespondStage
            if (ctx.factory().isRegistered("RespondStage")) {
                std::string respond_id = "auto_respond_" + std::to_string(ctx.iteration_count);
                if (!ctx.idExists(respond_id)) {
                    auto respond = ctx.factory().create("RespondStage", respond_id, {});
                    ctx.push(std::move(respond), AgentContext::Position::Back);
                }
            } else {
                // No RespondStage available — set final output directly
                ctx.should_stop  = true;
                ctx.final_output = observation;
                if (auto* bus = ctx.eventBus()) {
                    bus->emit(EventBus::makeEvent("agent_final_answer",
                        {{"stage", name}, {"answer", observation.dump()}}));
                }
            }
        } else {
            // Another iteration needed — push a new ReasonStage
            if (ctx.factory().isRegistered("ReasonStage")) {
                std::string reason_id = "auto_reason_" + std::to_string(ctx.iteration_count);
                if (!ctx.idExists(reason_id)) {
                    auto reason = ctx.factory().create("ReasonStage", reason_id,
                                                       {{"task", next_task}});
                    ctx.push(std::move(reason), AgentContext::Position::Back);
                }
            }
        }

        result.success = true;
        result.output  = observation;

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] ObserveStage(" << id << ") exception: " << e.what() << "\n";
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

std::string ObserveStage::description() const {
    return "Step 5 — Observe: review execution results and decide whether to respond or iterate";
}

void registerObserveStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "ObserveStage",
            "Step 5: inspects plan execution results; chains to RespondStage (done) or pushes a new ReasonStage (iterate)",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"properties", {
                 {"plan_results", {{"type", "array"}, {"description", "$ref-resolved outputs of all plan items"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<ObserveStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
