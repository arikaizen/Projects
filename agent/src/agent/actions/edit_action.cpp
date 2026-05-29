#include "edit_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult EditAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved   = ctx.resolveReferences(inputs);
        auto path       = resolved.at("path").get<std::string>();
        auto old_string = resolved.at("old_string").get<std::string>();
        auto new_string = resolved.at("new_string").get<std::string>();

        std::cerr << "[ACTION:" << name << "] editing file: " << path << "\n";

        // Read existing content.
        std::ifstream in_file(path, std::ios::in | std::ios::binary);
        if (!in_file.is_open()) {
            result.success = false;
            result.error   = "Cannot open file for reading: " + path;
            result.output  = {{"path", path}, {"replaced", false}};
            result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
            result.timestamp = system_clock::now();
            return result;
        }

        std::ostringstream ss;
        ss << in_file.rdbuf();
        in_file.close();
        std::string content = ss.str();

        // Find and replace first occurrence.
        auto pos = content.find(old_string);
        if (pos == std::string::npos) {
            result.success = true; // not a hard failure
            result.error   = "old_string not found in file: " + path;
            result.output  = {{"path", path}, {"replaced", false}};
        } else {
            content.replace(pos, old_string.size(), new_string);

            // Write back.
            std::ofstream out_file(path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!out_file.is_open()) {
                result.success = false;
                result.error   = "Cannot open file for writing: " + path;
                result.output  = {{"path", path}, {"replaced", false}};
            } else {
                out_file.write(content.data(), static_cast<std::streamsize>(content.size()));
                if (out_file.fail()) {
                    result.success = false;
                    result.error   = "Write failed for: " + path;
                    result.output  = {{"path", path}, {"replaced", false}};
                } else {
                    out_file.close();
                    result.success = true;
                    result.output  = {{"path", path}, {"replaced", true}};
                }
            }
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerEditAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "EditAction",
        "Replace the first occurrence of old_string with new_string in a file.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"path", "old_string", "new_string"}},
            {"properties", {
                {"path",       {{"type", "string"}, {"description", "File to edit."}}},
                {"old_string", {{"type", "string"}, {"description", "Exact string to find and replace."}}},
                {"new_string", {{"type", "string"}, {"description", "Replacement string."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<EditAction>(std::move(id), "EditAction", std::move(inputs));
    });
}

} // namespace agent
