#include "validate_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include "agent/agent_logger.hpp"
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>

namespace agent {

static const char* CORRECTIVE_SCHEMA_STR = R"({
  "type": "array",
  "description": "Corrective work items to inject when validation fails",
  "items": {
    "type": "object",
    "required": ["name", "id", "inputs"],
    "properties": {
      "name": {"type": "string"},
      "id": {"type": "string"},
      "inputs": {"type": "object"}
    }
  }
})";

ValidateStage::ValidateStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "ValidateStage", std::move(inputs)) {}

WorkResult ValidateStage::execute(AgentContext& ctx) {
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
        // Locate the target result
        const WorkResult* target = nullptr;
        if (inputs.contains("target_id") && inputs["target_id"].is_string()) {
            const std::string target_id = inputs["target_id"].get<std::string>();
            target = ctx.resultById(target_id);
            if (!target) {
                throw std::runtime_error("ValidateStage: no result found for target_id '" + target_id + "'");
            }
        } else {
            target = ctx.lastResult();
            if (!target) {
                throw std::runtime_error("ValidateStage: no previous result available and no target_id provided");
            }
        }

        if (!inputs.contains("criteria") || !inputs["criteria"].is_string()) {
            throw std::invalid_argument("ValidateStage requires 'criteria' string input");
        }
        const std::string criteria      = inputs["criteria"].get<std::string>();
        const std::string target_output = target->output.dump(2);
        const bool corrective_injection = inputs.value("corrective_injection", false);

        // Build template vars — include catalog when corrective injection may be needed
        std::map<std::string, std::string> template_vars;
        template_vars["TARGET_OUTPUT"] = target_output;
        template_vars["CRITERIA"]      = criteria;

        if (corrective_injection) {
            template_vars["CATALOG"]       = ctx.factory().toCatalogJson().dump(2);
            template_vars["OUTPUT_SCHEMA"] = CORRECTIVE_SCHEMA_STR;
        }

        const std::string prompt_name = corrective_injection
            ? "validate_stage_corrective"
            : "validate_stage";

        std::string system_prompt;
        // Try the corrective-specific template first; fall back gracefully to the base template
        // if the corrective variant is not present on disk.
        try {
            system_prompt = ctx.promptLoader().render(prompt_name, template_vars);
        } catch (const std::exception&) {
            // Fallback: always render the base validate_stage template with whatever vars we have
            system_prompt = ctx.promptLoader().render("validate_stage", template_vars);
        }

        std::string user_msg = "Validate the target output against the provided criteria now.";

        std::cerr << "[STAGE] ValidateStage(" << id << ") calling LLM "
                  << "(target: " << target->item_id << ")\n";

        // json_mode = true: expect {"valid": bool, "reason": "..."}
        auto resp = llmComplete(ctx, {system_prompt, user_msg, /*json_mode=*/true, 0.2f, 2048});
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

        // Parse validation response
        nlohmann::json validation;
        try {
            validation = nlohmann::json::parse(resp.content);
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

        if (!validation.is_object()) {
            result.success = false;
            result.error   = std::string("Validation response must be a JSON object, got: ") + validation.type_name();
            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (auto* bus = ctx.eventBus()) {
                bus->emit(EventBus::makeEvent("stage_error", {{"stage", name}, {"id", id}, {"error", result.error}}));
            }
            return result;
        }

        const bool   is_valid = validation.value("valid", false);
        const std::string reason = validation.value("reason", std::string{});

        std::cerr << "[STAGE] ValidateStage(" << id << ") result: "
                  << (is_valid ? "VALID" : "INVALID") << " — " << reason << "\n";

        // If invalid and corrective injection is requested, ask the LLM for corrective items
        if (!is_valid && corrective_injection) {
            std::cerr << "[STAGE] ValidateStage(" << id << ") requesting corrective plan\n";

            // Build a corrective prompt — provide the validation reason as additional context
            std::map<std::string, std::string> corrective_vars = template_vars;
            corrective_vars["VALIDATION_REASON"] = reason;

            std::string corrective_system;
            try {
                corrective_system = ctx.promptLoader().render("validate_stage_corrective_plan", corrective_vars);
            } catch (const std::exception&) {
                // Fallback: embed context directly into the user message
                corrective_system = ctx.promptLoader().render("validate_stage", corrective_vars);
            }

            std::string corrective_user =
                "The previous result failed validation. Reason: " + reason +
                "\n\nProduce a corrective plan as a JSON array of work items.";

            auto corrective_resp = llmComplete(ctx,
                {corrective_system, corrective_user, /*json_mode=*/true, 0.3f, 4096});

            if (corrective_resp.success) {
                nlohmann::json corrective_plan;
                bool parse_ok = false;
                try {
                    corrective_plan = nlohmann::json::parse(corrective_resp.content);
                    parse_ok = corrective_plan.is_array();
                } catch (...) {}

                if (parse_ok) {
                    // Validate and push corrective items to FRONT
                    std::set<std::string> seen_ids;
                    for (auto& r : ctx.history()) seen_ids.insert(r.item_id);

                    bool corrective_ok = true;
                    std::vector<std::pair<std::string, std::pair<std::string, nlohmann::json>>> corrective_entries;

                    for (const auto& item_json : corrective_plan) {
                        if (!item_json.is_object() ||
                            !item_json.contains("name") || !item_json["name"].is_string() ||
                            !item_json.contains("id")   || !item_json["id"].is_string()) {
                            std::cerr << "[STAGE] ValidateStage: corrective plan item malformed, skipping\n";
                            corrective_ok = false;
                            break;
                        }
                        const std::string cname   = item_json["name"].get<std::string>();
                        const std::string cid     = item_json["id"].get<std::string>();
                        nlohmann::json    cinputs  = item_json.value("inputs", nlohmann::json::object());

                        if (!ctx.factory().isRegistered(cname)) {
                            std::cerr << "[STAGE] ValidateStage: corrective item type '" << cname << "' not registered\n";
                            corrective_ok = false;
                            break;
                        }
                        if (ctx.idExists(cid) || seen_ids.count(cid)) {
                            std::cerr << "[STAGE] ValidateStage: corrective item id '" << cid << "' duplicated\n";
                            corrective_ok = false;
                            break;
                        }
                        auto temp = ctx.factory().create(cname, cid, cinputs);
                        for (const auto& dep : temp->dependencies()) {
                            if (!seen_ids.count(dep)) {
                                std::cerr << "[STAGE] ValidateStage: corrective ref '$" << dep << "' unsatisfied\n";
                                corrective_ok = false;
                                break;
                            }
                        }
                        if (!corrective_ok) break;

                        seen_ids.insert(cid);
                        corrective_entries.push_back({cname, {cid, cinputs}});
                    }

                    if (corrective_ok && !corrective_entries.empty()) {
                        // Push to FRONT in reverse order to preserve plan order
                        for (int i = static_cast<int>(corrective_entries.size()) - 1; i >= 0; --i) {
                            const auto& [cname, id_inputs] = corrective_entries[i];
                            auto work_item = ctx.factory().create(cname, id_inputs.first, id_inputs.second);
                            ctx.push(std::move(work_item), AgentContext::Position::Front);
                        }
                        std::cerr << "[STAGE] ValidateStage(" << id << ") injected "
                                  << corrective_entries.size() << " corrective item(s)\n";
                        if (auto* bus = ctx.eventBus()) {
                            bus->emit(EventBus::makeEvent("corrective_injection", {
                                {"stage", name}, {"id", id},
                                {"count", corrective_entries.size()},
                                {"reason", reason}
                            }));
                        }
                    }
                } else {
                    std::cerr << "[STAGE] ValidateStage(" << id << ") corrective plan parse failed\n";
                }
            } else {
                std::cerr << "[STAGE] ValidateStage(" << id << ") corrective LLM call failed: "
                          << corrective_resp.error << "\n";
            }
        }

        result.success = true;
        result.output  = {{"valid", is_valid}, {"reason", reason}};

        if (auto* bus = ctx.eventBus()) {
            bus->emit(EventBus::makeEvent("validation_result", {
                {"stage", name}, {"id", id},
                {"target_id", target->item_id},
                {"valid", is_valid},
                {"reason", reason}
            }));
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] ValidateStage(" << id << ") exception: " << e.what() << "\n";
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

std::string ValidateStage::description() const {
    return "LLM-powered validation stage: checks a previous result against criteria and optionally injects corrective items";
}

void registerValidateStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "ValidateStage",
            "LLM-powered validation stage with optional corrective injection on failure",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"required", {"criteria"}},
             {"properties", {
                 {"target_id",             {{"type", "string"},  {"description", "Id of result to validate; defaults to last result"}}},
                 {"criteria",              {{"type", "string"},  {"description", "Validation criteria description for the LLM"}}},
                 {"corrective_injection",  {{"type", "boolean"}, {"description", "If true and invalid, inject corrective work items to fix the failure"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<ValidateStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
