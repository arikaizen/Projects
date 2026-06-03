#include "replay_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>
#include <map>

namespace agent {

ReplayStage::ReplayStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "ReplayStage", std::move(inputs)) {}

nlohmann::json ReplayStage::remapRefs(const nlohmann::json& j,
                                       const std::map<std::string, std::string>& id_map)
{
    if (j.is_string()) {
        const auto& s = j.get_ref<const std::string&>();
        if (!s.empty() && s[0] == '$') {
            auto dot      = s.find('.');
            std::string ref_id = (dot == std::string::npos) ? s.substr(1) : s.substr(1, dot - 1);
            std::string suffix = (dot == std::string::npos) ? "" : s.substr(dot);
            auto it = id_map.find(ref_id);
            if (it != id_map.end()) return "$" + it->second + suffix;
        }
        return j;
    }
    if (j.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : j) arr.push_back(remapRefs(item, id_map));
        return arr;
    }
    if (j.is_object()) {
        nlohmann::json obj = nlohmann::json::object();
        for (const auto& [k, v] : j.items()) obj[k] = remapRefs(v, id_map);
        return obj;
    }
    return j;
}

WorkResult ReplayStage::execute(AgentContext& ctx) {
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
        nlohmann::json steps = inputs.value("steps", nlohmann::json::array());
        if (!steps.is_array() || steps.empty()) {
            result.success = false;
            result.error   = "ReplayStage: no steps to replay";
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error",
                    {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        // Build ID remap: old_id → "replay_N"
        std::map<std::string, std::string> id_map;
        int n = 0;
        for (const auto& step : steps) {
            if (step.is_object() && step.contains("id") && step["id"].is_string()) {
                std::string old_id = step["id"].get<std::string>();
                id_map[old_id] = "replay_" + std::to_string(n++);
            }
        }

        // Push plan items with remapped IDs and rewritten $refs in inputs
        std::vector<std::string> pushed_ids;
        for (const auto& step : steps) {
            if (!step.is_object()) continue;
            if (!step.contains("name") || !step["name"].is_string()) continue;
            if (!step.contains("id")   || !step["id"].is_string())   continue;

            const std::string item_name = step["name"].get<std::string>();
            const std::string old_id    = step["id"].get<std::string>();
            std::string       new_id    = id_map.count(old_id) ? id_map.at(old_id) : old_id;

            // Avoid collision if new_id already exists in history
            if (ctx.idExists(new_id)) {
                new_id = new_id + "_r" + std::to_string(ctx.iteration_count);
                id_map[old_id] = new_id;
            }

            if (!ctx.factory().isRegistered(item_name)) {
                std::cerr << "[STAGE] ReplayStage: unknown item type '" << item_name << "' — skipping\n";
                continue;
            }

            nlohmann::json item_inputs = remapRefs(
                step.value("inputs", nlohmann::json::object()), id_map);

            auto work_item = ctx.factory().create(item_name, new_id, item_inputs);
            ctx.push(std::move(work_item), AgentContext::Position::Back);
            pushed_ids.push_back(new_id);
        }

        std::cerr << "[STAGE] ReplayStage(" << id << ") replayed " << pushed_ids.size() << " item(s)\n";

        // Write original steps to blackboard so ObserveStage can re-save to cache
        if (auto* bb = ctx.blackboard())
            bb->write("agent:last_plan", steps);

        // Push ObserveStage with $ref deps on all replayed items
        if (!pushed_ids.empty() && ctx.factory().isRegistered("ObserveStage")) {
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
        result.output  = {{"replayed_count", pushed_ids.size()}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] ReplayStage(" << id << ") exception: " << e.what() << "\n";
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

std::string ReplayStage::description() const {
    return "Replays a cached plan with remapped IDs — no LLM call needed";
}

void registerReplayStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "ReplayStage",
            "Replays a cached successful plan with remapped IDs; pushes ObserveStage to verify",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"properties", {
                 {"steps", {{"type", "array"},  {"description", "Cached plan steps from PlanCache"}}},
                 {"task",  {{"type", "string"}, {"description", "Current task string"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<ReplayStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
