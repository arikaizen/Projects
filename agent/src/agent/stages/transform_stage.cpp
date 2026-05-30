#include "transform_stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/event_bus.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace agent {

TransformStage::TransformStage(std::string id, nlohmann::json inputs)
    : Stage(std::move(id), "TransformStage", std::move(inputs)) {}

WorkResult TransformStage::execute(AgentContext& ctx) {
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
        // Resolve $ref values in inputs before extracting fields
        nlohmann::json resolved = ctx.resolveReferences(inputs);

        if (!resolved.contains("instruction") || !resolved["instruction"].is_string()) {
            throw std::invalid_argument("TransformStage requires 'instruction' string input");
        }
        if (!resolved.contains("text") || !resolved["text"].is_string()) {
            throw std::invalid_argument("TransformStage requires 'text' string input (or a $ref resolving to a string)");
        }

        const std::string instruction = resolved["instruction"].get<std::string>();
        const std::string input_text  = resolved["text"].get<std::string>();

        std::string system_prompt = ctx.promptLoader().render("transform_stage", {
            {"INSTRUCTION", instruction},
            {"INPUT_TEXT",  input_text}
        });

        std::string user_msg = "Apply the transformation now.";

        std::cerr << "[STAGE] TransformStage(" << id << ") calling LLM\n";

        // Free-text output: json_mode = false
        auto resp = ctx.llm().complete({system_prompt, user_msg, /*json_mode=*/false, 0.5f, 4096});
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

        std::cerr << "[STAGE] TransformStage(" << id << ") completed transformation\n";
        result.success = true;
        result.output  = {{"transformed_text", resp.content}};

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
        std::cerr << "[STAGE] TransformStage(" << id << ") exception: " << e.what() << "\n";
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

std::string TransformStage::description() const {
    return "LLM-powered text transformation: applies a natural-language instruction to an input text";
}

void registerTransformStage(WorkFactory& factory) {
    factory.registerItem(
        WorkItemSpec{
            "TransformStage",
            "LLM-powered text transformation stage",
            WorkItem::Kind::Stage,
            {{"type", "object"},
             {"required", {"instruction", "text"}},
             {"properties", {
                 {"instruction", {{"type", "string"}, {"description", "Transformation instruction for the LLM"}}},
                 {"text",        {{"type", "string"}, {"description", "Text to transform (or a $ref to a string field)"}}}
             }}}
        },
        [](std::string id, nlohmann::json inp) {
            return std::make_unique<TransformStage>(std::move(id), std::move(inp));
        }
    );
}

} // namespace agent
