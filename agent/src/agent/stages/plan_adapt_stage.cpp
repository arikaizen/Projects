#include "plan_adapt_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>
#include <set>

namespace agent {

static const char* ADAPT_OUTPUT_SCHEMA = R"({
  "type": "array",
  "description": "Adapted plan: same format as ReasonStage output. Steps unchanged from the cached plan may be included as-is; steps that need to change should be replaced.",
  "items": {
    "type": "object",
    "required": ["name", "id", "inputs"],
    "properties": {
      "name":         {"type": "string"},
      "id":           {"type": "string"},
      "inputs":       {"type": "object"},
      "final_answer": {"type": "string"}
    }
  }
})";

PlanAdaptStage::PlanAdaptStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "PlanAdaptStage", std::move(inputs)) {}

WorkResult PlanAdaptStage::execute(AgentContext& ctx) {
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

    try {
        std::string task = ctx.config().task;
        if (inputs.contains("task") && inputs["task"].is_string())
            task = inputs["task"].get<std::string>();

        std::string cached_plan_str = inputs.value("cached_plan", nlohmann::json::array()).dump(2);
        std::string changed_str     = inputs.value("changed_aspects", nlohmann::json::array()).dump(2);
        std::string catalog         = ctx.factory().toCatalogJson().dump(2);

        std::string system_prompt = ctx.promptLoader().render("plan_adapt_stage", {
            {"TASK",             task},
            {"CACHED_PLAN",      cached_plan_str},
            {"CHANGED_ASPECTS",  changed_str},
            {"CATALOG",          catalog},
            {"OUTPUT_SCHEMA",    ADAPT_OUTPUT_SCHEMA}
        });
        std::string user_msg = "Produce the adapted plan now.";

        std::cerr << "[STAGE] PlanAdaptStage(" << id << ") calling LLM\n";
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.3f, 4096});
        if (!resp.success) {
            result.success = false;
            result.error   = "LLM call failed: " + resp.error;
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error",
                    {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(resp.content);
        } catch (const std::exception& ex) {
            result.success = false;
            result.error   = std::string("invalid JSON from LLM: ") + ex.what();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error",
                    {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        if (!parsed.is_array()) {
            result.success = false;
            result.error   = std::string("expected JSON array, got ") + parsed.type_name();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error",
                    {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        // Validate and push adapted plan items (mirrors ReasonStage logic)
        std::set<std::string> seen_ids;
        for (const auto& r : ctx.history()) seen_ids.insert(r.item_id);

        std::vector<std::string> pushed_ids;

        for (const auto& item_json : parsed) {
            if (!item_json.is_object()) continue;
            if (!item_json.contains("name") || !item_json["name"].is_string()) continue;
            if (!item_json.contains("id")   || !item_json["id"].is_string())   continue;

            const std::string item_name   = item_json["name"].get<std::string>();
            const std::string item_id     = item_json["id"].get<std::string>();
            nlohmann::json    item_inputs = item_json.value("inputs", nlohmann::json::object());

            if (!ctx.factory().isRegistered(item_name)) {
                std::cerr << "[STAGE] PlanAdaptStage: unknown type '" << item_name << "' — skipping\n";
                continue;
            }
            if (ctx.idExists(item_id) || seen_ids.count(item_id)) {
                std::cerr << "[STAGE] PlanAdaptStage: duplicate id '" << item_id << "' — skipping\n";
                continue;
            }
            seen_ids.insert(item_id);

            if (item_json.contains("final_answer") && item_json["final_answer"].is_string()) {
                auto wi = ctx.factory().create(item_name, item_id, item_inputs);
                ctx.push(std::move(wi), AgentContext::Position::Back);
                ctx.should_stop  = true;
                ctx.final_output = {{"answer", item_json["final_answer"].get<std::string>()}};
                if (auto* bus = ctx.eventBus())
                    bus->emit(EventBus::makeEvent("agent_final_answer",
                        {{"stage", name}, {"answer", item_json["final_answer"]}}));
                result.success = true;
                result.output  = {{"adapted_size", parsed.size()}, {"plan", parsed}};
                result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);
                if (auto* bus = ctx.eventBus())
                    bus->emit(EventBus::makeEvent("stage_done",
                        {{"stage", name}, {"id", id}, {"success", true}}));
                return result;
            }

            auto wi = ctx.factory().create(item_name, item_id, item_inputs);
            ctx.push(std::move(wi), AgentContext::Position::Back);
            pushed_ids.push_back(item_id);
        }

        std::cerr << "[STAGE] PlanAdaptStage(" << id << ") pushed " << pushed_ids.size() << " adapted item(s)\n";

        // Save adapted plan to blackboard for ObserveStage to cache on success
        if (auto* bb = ctx.blackboard())
            bb->write("agent:last_plan", parsed);

        // Push ObserveStage with $ref deps on all adapted items
        if (!ctx.should_stop && !pushed_ids.empty() && ctx.factory().isRegistered("ObserveStage")) {
            std::string obs_id = "observe_" + id;
            if (!ctx.idExists(obs_id)) {
                nlohmann::json obs_inputs;
                nlohmann::json refs = nlohmann::json::array();
                for (const auto& pid : pushed_ids) refs.push_back("$" + pid);
                obs_inputs["plan_results"] = refs;
                auto observe = ctx.factory().create("ObserveStage", obs_id, obs_inputs);
                ctx.push(std::move(observe), AgentContext::Position::Back);
            }
        }

        result.success = true;
        result.output  = {{"adapted_size", pushed_ids.size()}, {"plan", parsed}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] PlanAdaptStage(" << id << ") exception: " << e.what() << "\n";
        if (auto* bus = ctx.eventBus())
            bus->emit(EventBus::makeEvent("stage_error",
                {{"stage", name}, {"id", id}, {"error", result.error}}));
    }

    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    if (auto* bus = ctx.eventBus())
        bus->emit(EventBus::makeEvent("stage_done",
            {{"stage", name}, {"id", id}, {"success", result.success}}));
    return result;
}

std::string PlanAdaptStage::description() const {
    return "Adapts a cached plan to changed parameters using LLM reasoning";
}

void registerPlanAdaptStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "PlanAdaptStage",
            "Calls the LLM to adapt a cached plan to changed task parameters; pushes adapted plan + ObserveStage",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"properties", {
                 {"cached_plan",     {{"type", "array"},  {"description", "Original cached plan steps"}}},
                 {"changed_aspects", {{"type", "array"},  {"description", "What changed since the cached run"}}},
                 {"task",            {{"type", "string"}, {"description", "Current task"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<PlanAdaptStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
