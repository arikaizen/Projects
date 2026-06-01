#include "locate_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include <chrono>
#include <iostream>
#include <set>

namespace agent {

static const char* LOCATE_ACTIONS_SCHEMA = R"({
  "type": "object",
  "required": ["actions"],
  "properties": {
    "actions": {
      "type": "array",
      "description": "Locate actions to execute. Each must be a registered Action type.",
      "items": {
        "type": "object",
        "required": ["name", "id", "inputs"],
        "properties": {
          "name":   {"type": "string"},
          "id":     {"type": "string"},
          "inputs": {"type": "object"}
        }
      }
    }
  }
})";

LocateStage::LocateStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "LocateStage", std::move(inputs)) {}

WorkResult LocateStage::execute(AgentContext& ctx) {
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

        std::string understanding_str = "{}";
        std::string orientation_str   = "{}";
        if (ctx.blackboard()) {
            if (auto v = ctx.blackboard()->read("agent:understanding")) understanding_str = v->dump(2);
            if (auto v = ctx.blackboard()->read("agent:orientation"))   orientation_str   = v->dump(2);
        }

        std::string system_prompt = ctx.promptLoader().render("locate_stage", {
            {"TASK",          task},
            {"UNDERSTANDING", understanding_str},
            {"ORIENTATION",   orientation_str},
            {"OUTPUT_SCHEMA", LOCATE_ACTIONS_SCHEMA}
        });
        std::string user_msg = "Identify the resources to locate and return the action plan now.";

        std::cerr << "[STAGE] LocateStage(" << id << ") calling LLM\n";
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

        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(resp.content);
        } catch (const std::exception& ex) {
            result.success = false;
            result.error   = std::string("LLM returned invalid JSON: ") + ex.what();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus())
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            return result;
        }

        // Push each locate action and collect their IDs for ReadStage $refs
        std::set<std::string>  seen_ids;
        std::vector<std::string> pushed_ids;
        for (const auto& r : ctx.history()) seen_ids.insert(r.item_id);

        const auto& actions = parsed.value("actions", nlohmann::json::array());
        for (const auto& a : actions) {
            if (!a.is_object()) continue;
            if (!a.contains("name") || !a["name"].is_string()) continue;
            if (!a.contains("id")   || !a["id"].is_string())   continue;

            const std::string act_name   = a["name"].get<std::string>();
            const std::string act_id     = a["id"].get<std::string>();
            nlohmann::json    act_inputs = a.value("inputs", nlohmann::json::object());

            if (!ctx.factory().isRegistered(act_name)) {
                std::cerr << "[STAGE] LocateStage: unknown action type '" << act_name << "', skipping\n";
                continue;
            }
            if (ctx.idExists(act_id) || seen_ids.count(act_id)) {
                std::cerr << "[STAGE] LocateStage: duplicate id '" << act_id << "', skipping\n";
                continue;
            }

            seen_ids.insert(act_id);
            auto item = ctx.factory().create(act_name, act_id, act_inputs);
            ctx.push(std::move(item), AgentContext::Position::Back);
            pushed_ids.push_back(act_id);
        }

        // Push ReadStage with $ref dependencies on all locate action IDs
        if (ctx.factory().isRegistered("ReadStage")) {
            std::string read_id = "auto_read";
            if (!ctx.idExists(read_id)) {
                nlohmann::json read_inputs;
                read_inputs["task"] = task;
                if (!pushed_ids.empty()) {
                    // Build object {"id": "$id"} so BatchExecutor resolves them and
                    // registers them as DAG dependencies before ReadStage executes.
                    nlohmann::json refs = nlohmann::json::object();
                    for (const auto& aid : pushed_ids)
                        refs[aid] = "$" + aid;
                    read_inputs["locate_results"] = refs;
                }
                auto read = ctx.factory().create("ReadStage", read_id, read_inputs);
                ctx.push(std::move(read), AgentContext::Position::Back);
            }
        }

        std::cerr << "[STAGE] LocateStage(" << id << ") pushed "
                  << pushed_ids.size() << " locate action(s)\n";
        result.success = true;
        result.output  = {{"locate_action_count", static_cast<int>(pushed_ids.size())},
                          {"locate_action_ids", pushed_ids}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] LocateStage(" << id << ") exception: " << e.what() << "\n";
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

std::string LocateStage::description() const {
    return "Step 2B — Locate: identify and execute searches to find relevant resources";
}

void registerLocateStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "LocateStage",
            "Step 2B: asks the LLM which resources to locate; pushes locate actions and chains to ReadStage",
            WorkItem::Kind::Stage,
            {{"type", "object"}, {"properties", {{"task", {{"type", "string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<LocateStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
