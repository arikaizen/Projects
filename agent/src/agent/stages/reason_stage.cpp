#include "reason_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>
#include <set>
#include <sstream>

namespace agent {

static const char* OUTPUT_SCHEMA_STR = R"({
  "type": "array",
  "description": "Ordered list of work items. May be empty if final_answer is provided.",
  "items": {
    "type": "object",
    "required": ["name", "id", "inputs"],
    "properties": {
      "name": {"type": "string"},
      "id": {"type": "string"},
      "inputs": {"type": "object"},
      "final_answer": {"type": "string", "description": "Set on last item to terminate the agent"}
    }
  }
})";

ReasonStage::ReasonStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "ReasonStage", std::move(inputs)) {}

WorkResult ReasonStage::execute(AgentContext& ctx) {
    auto start = std::chrono::steady_clock::now();
    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Stage";
    result.timestamp = std::chrono::system_clock::now();

    // Emit stage-start event
    if (auto* bus = ctx.eventBus()) {
        bus->emit(EventBus::makeEvent("stage_start", {{"stage", name}, {"id", id}}));
    }
    if (auto* logger = ctx.logger())
        logger->stageStart(ctx.config().agent_id, name, id, inputs);

    try {
        // Gather template variables
        std::string catalog  = ctx.factory().toCatalogJson().dump(2);
        std::string history  = ctx.historySummaryJson(20).dump(2);
        std::string queue    = ctx.queueSummaryJson().dump(2);
        std::string task     = ctx.config().task;
        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        std::string system_prompt = ctx.promptLoader().render("reason_stage", {
            {"CATALOG",       catalog},
            {"HISTORY",       history},
            {"QUEUE",         queue},
            {"TASK",          task},
            {"OUTPUT_SCHEMA", OUTPUT_SCHEMA_STR}
        });

        std::string user_msg = "Based on the current state, produce your plan now.";

        std::cerr << "[STAGE] ReasonStage(" << id << ") calling LLM\n";
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.3f, 4096});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            }
            return result;
        }

        // Parse JSON response
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(resp.content);
        } catch (const std::exception& ex) {
            result.success = false;
            result.error   = std::string("LLM returned invalid JSON: ") + ex.what() +
                             " | content: " + resp.content.substr(0, 200);
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            }
            return result;
        }

        // Check for top-level final_answer object
        if (parsed.is_object() && parsed.contains("final_answer")) {
            std::cerr << "[STAGE] ReasonStage(" << id << ") received top-level final_answer\n";
            ctx.should_stop  = true;
            ctx.final_output = {{"answer", parsed["final_answer"]}};
            result.success   = true;
            result.output    = ctx.final_output;
            result.duration  = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("agent_final_answer", {{"stage", name}, {"answer", parsed["final_answer"]}}));
            }
            return result;
        }

        // Expect array plan
        if (!parsed.is_array()) {
            result.success = false;
            result.error   = std::string("LLM response must be JSON array, got: ") + parsed.type_name();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            }
            return result;
        }

        std::string plan_error;
        if (!validateAndPushPlan(parsed, ctx, plan_error)) {
            result.success = false;
            result.error   = plan_error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", plan_error}}));
            }
            return result;
        }

        // Save raw plan to blackboard so ObserveStage can persist it to PlanCache on success
        if (auto* bb = ctx.blackboard())
            bb->write("agent:last_plan", parsed);
        if (auto* logger = ctx.logger())
            logger->planPushed(ctx.config().agent_id, name, id, parsed);

        std::cerr << "[STAGE] ReasonStage(" << id << ") pushed " << parsed.size() << " plan item(s)\n";
        result.success = true;
        result.output  = {{"plan_size", parsed.size()}, {"plan", parsed}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] ReasonStage(" << id << ") exception: " << e.what() << "\n";
        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
        }
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

bool ReasonStage::validateAndPushPlan(const nlohmann::json& plan,
                                       AgentContext& ctx,
                                       std::string& error_out) {
    // Seed seen_ids with all history item ids
    std::set<std::string> seen_ids;
    for (auto& r : ctx.history()) {
        seen_ids.insert(r.item_id);
    }

    std::vector<std::string> pushed_ids;  // collected to wire ObserveStage deps

    for (const auto& item_json : plan) {
        if (!item_json.is_object()) {
            error_out = "Plan items must be JSON objects";
            return false;
        }
        if (!item_json.contains("name") || !item_json["name"].is_string()) {
            error_out = "Plan item missing required 'name' string field";
            return false;
        }
        if (!item_json.contains("id") || !item_json["id"].is_string()) {
            error_out = "Plan item missing required 'id' string field";
            return false;
        }

        const std::string item_name   = item_json["name"].get<std::string>();
        const std::string item_id     = item_json["id"].get<std::string>();
        nlohmann::json    item_inputs = item_json.value("inputs", nlohmann::json::object());

        // 1. Type must be registered
        if (!ctx.factory().isRegistered(item_name)) {
            error_out = "Unknown work item type: '" + item_name + "'";
            return false;
        }

        // 2. ID must be unique across history and the current plan
        if (ctx.idExists(item_id) || seen_ids.count(item_id)) {
            error_out = "Duplicate item id: '" + item_id + "'";
            return false;
        }

        // 3. Validate $ref dependencies — each must point to a known id
        //    (history ids already in seen_ids; earlier plan items added below)
        auto temp_item = ctx.factory().create(item_name, item_id, item_inputs);
        for (const auto& dep_id : temp_item->dependencies()) {
            if (!seen_ids.count(dep_id)) {
                error_out = "Reference '$" + dep_id + "' in item '" + item_id +
                            "' is not satisfied (not in history or earlier in plan)";
                return false;
            }
        }

        // Mark id as seen so later items in this plan may reference it
        seen_ids.insert(item_id);

        // 4. If this item carries a final_answer, push it and stop plan processing
        if (item_json.contains("final_answer") && item_json["final_answer"].is_string()) {
            auto work_item = ctx.factory().create(item_name, item_id, item_inputs);
            ctx.push(std::move(work_item), AgentContext::Position::Back);
            ctx.should_stop  = true;
            ctx.final_output = {{"answer", item_json["final_answer"].get<std::string>()}};
            std::cerr << "[STAGE] ReasonStage plan contains final_answer — stopping after this item\n";
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("agent_final_answer",
                    {{"stage", name}, {"answer", item_json["final_answer"]}}));
            }
            return true;  // final_answer path: no ObserveStage needed
        }

        auto work_item = ctx.factory().create(item_name, item_id, item_inputs);
        ctx.push(std::move(work_item), AgentContext::Position::Back);
        pushed_ids.push_back(item_id);
    }

    // Step 5 — Observe: push ObserveStage with $ref dependencies on every plan
    // item so it runs only after all of them complete (BatchExecutor DAG ordering).
    // Skipped when the plan was empty or should_stop was already set above.
    if (!ctx.should_stop && !pushed_ids.empty() &&
        ctx.factory().isRegistered("ObserveStage")) {
        std::string obs_id = "observe_" + id;
        if (!ctx.idExists(obs_id)) {
            nlohmann::json obs_inputs;
            nlohmann::json refs = nlohmann::json::array();
            for (const auto& pid : pushed_ids)
                refs.push_back("$" + pid);
            obs_inputs["plan_results"] = refs;

            auto observe = ctx.factory().create("ObserveStage", obs_id, obs_inputs);
            ctx.push(std::move(observe), AgentContext::Position::Back);
            std::cerr << "[STAGE] ReasonStage(" << id << ") queued ObserveStage '"
                      << obs_id << "' with " << pushed_ids.size() << " dep(s)\n";
        }
    }

    return true;
}

std::string ReasonStage::description() const {
    return "Primary reasoning stage: plans and injects work items based on current agent state";
}

void registerReasonStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "ReasonStage",
            "Primary LLM reasoning stage that produces a plan of work items",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<ReasonStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
