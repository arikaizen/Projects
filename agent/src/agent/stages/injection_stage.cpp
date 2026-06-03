#include "injection_stage.hpp"
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

static const char* INJECTION_OUTPUT_SCHEMA_STR = R"({
  "type": "array",
  "description": "Ordered list of work items to inject at the front of the queue. May be empty if final_answer is provided.",
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

InjectionStage::InjectionStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "InjectionStage", std::move(inputs)) {}

WorkResult InjectionStage::execute(AgentContext& ctx) {
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
        // Resolve the target result: prefer inputs["target_id"] if present
        const WorkResult* prev = nullptr;
        if (inputs.contains("target_id") && inputs["target_id"].is_string()) {
            const std::string target_id = inputs["target_id"].get<std::string>();
            prev = ctx.resultById(target_id);
            if (!prev) {
                result.success = false;
                result.error   = "InjectionStage: no result found for target_id '" + target_id + "'";
                result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);
                std::cerr << "[STAGE] " << result.error << "\n";
                if (auto* bus = ctx.eventBus()) {
                    bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
                }
                return result;
            }
        } else {
            prev = ctx.lastResult();
            if (!prev) {
                result.success = false;
                result.error   = "InjectionStage: no previous result available";
                result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);
                std::cerr << "[STAGE] " << result.error << "\n";
                if (auto* bus = ctx.eventBus()) {
                    bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
                }
                return result;
            }
        }

        // Build template variables
        std::string catalog          = ctx.factory().toCatalogJson().dump(2);
        std::string history          = ctx.historySummaryJson(20).dump(2);
        std::string queue            = ctx.queueSummaryJson().dump(2);
        std::string task             = ctx.config().task;
        std::string previous_result  = prev->output.dump(2);

        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        std::string system_prompt = ctx.promptLoader().render("injection_stage", {
            {"CATALOG",         catalog},
            {"HISTORY",         history},
            {"QUEUE",           queue},
            {"TASK",            task},
            {"PREVIOUS_RESULT", previous_result},
            {"OUTPUT_SCHEMA",   INJECTION_OUTPUT_SCHEMA_STR}
        });

        std::string user_msg = "Given the previous result, decide what to inject next.";

        std::cerr << "[STAGE] InjectionStage(" << id << ") calling LLM "
                  << "(previous item: " << prev->item_id << ")\n";

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

        // Parse response
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

        // Top-level final_answer object
        if (parsed.is_object() && parsed.contains("final_answer")) {
            std::cerr << "[STAGE] InjectionStage(" << id << ") received top-level final_answer\n";
            ctx.should_stop  = true;
            ctx.final_output = {{"answer", parsed["final_answer"]}};
            result.success   = true;
            result.output    = ctx.final_output;
            result.duration  = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("agent_final_answer",
                    {{"stage", name}, {"answer", parsed["final_answer"]}}));
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

        std::cerr << "[STAGE] InjectionStage(" << id << ") injected "
                  << parsed.size() << " item(s) to FRONT\n";
        result.success = true;
        result.output  = {{"injected_count", parsed.size()}, {"plan", parsed}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] InjectionStage(" << id << ") exception: " << e.what() << "\n";
        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
        }
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

bool InjectionStage::validateAndPushPlan(const nlohmann::json& plan,
                                          AgentContext& ctx,
                                          std::string& error_out) {
    // Seed seen_ids with all history item ids
    std::set<std::string> seen_ids;
    for (auto& r : ctx.history()) {
        seen_ids.insert(r.item_id);
    }

    // Collect items in reverse so we can push to FRONT and preserve order.
    // Push order: last item first -> each subsequent push goes to front -> final order correct.
    // We validate left-to-right first, collecting the items, then push in reverse.
    struct PlanEntry {
        std::string    item_name;
        std::string    item_id;
        nlohmann::json item_inputs;
        bool           is_final{false};
        std::string    final_answer;
    };

    std::vector<PlanEntry> entries;
    entries.reserve(plan.size());

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

        PlanEntry entry;
        entry.item_name   = item_json["name"].get<std::string>();
        entry.item_id     = item_json["id"].get<std::string>();
        entry.item_inputs = item_json.value("inputs", nlohmann::json::object());

        if (!ctx.factory().isRegistered(entry.item_name)) {
            error_out = "Unknown work item type: '" + entry.item_name + "'";
            return false;
        }
        if (ctx.idExists(entry.item_id) || seen_ids.count(entry.item_id)) {
            error_out = "Duplicate item id: '" + entry.item_id + "'";
            return false;
        }

        // Validate $ref dependencies
        auto temp_item = ctx.factory().create(entry.item_name, entry.item_id, entry.item_inputs);
        for (const auto& dep_id : temp_item->dependencies()) {
            if (!seen_ids.count(dep_id)) {
                error_out = "Reference '$" + dep_id + "' in item '" + entry.item_id +
                            "' is not satisfied (not in history or earlier in plan)";
                return false;
            }
        }

        seen_ids.insert(entry.item_id);

        if (item_json.contains("final_answer") && item_json["final_answer"].is_string()) {
            entry.is_final    = true;
            entry.final_answer = item_json["final_answer"].get<std::string>();
        }

        entries.push_back(std::move(entry));

        if (entries.back().is_final) {
            break;  // No further items after a final_answer
        }
    }

    // Push to FRONT in reverse order so execution order matches plan order
    for (int i = static_cast<int>(entries.size()) - 1; i >= 0; --i) {
        const auto& entry = entries[i];
        auto work_item = ctx.factory().create(entry.item_name, entry.item_id, entry.item_inputs);
        ctx.push(std::move(work_item), AgentContext::Position::Front);
    }

    // Handle final_answer from the last (or only final) entry
    if (!entries.empty() && entries.back().is_final) {
        ctx.should_stop  = true;
        ctx.final_output = {{"answer", entries.back().final_answer}};
        std::cerr << "[STAGE] InjectionStage plan contains final_answer — stopping after injected items\n";
        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent("agent_final_answer",
                {{"stage", "InjectionStage"}, {"answer", entries.back().final_answer}}));
        }
    }

    return true;
}

std::string InjectionStage::description() const {
    return "Meta-stage: given the previous result, injects follow-up work items at the front of the queue";
}

void registerInjectionStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "InjectionStage",
            "Meta-stage that injects follow-up work items based on the previous result",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {
                {"target_id", {{"type", "string"}, {"description", "Id of result to inspect; defaults to last result"}}},
                {"task",      {{"type", "string"}}}
            }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<InjectionStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
