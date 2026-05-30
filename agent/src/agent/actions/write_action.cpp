#include "write_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult WriteAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto path     = resolved.at("path").get<std::string>();
        auto content  = resolved.at("content").get<std::string>();

        std::cerr << "[ACTION:" << name << "] writing file: " << path
                  << " (" << content.size() << " bytes)\n";

        // Create parent directories if they don't exist.
        std::filesystem::path fs_path(path);
        if (fs_path.has_parent_path()) {
            std::filesystem::create_directories(fs_path.parent_path());
        }

        std::ofstream file(path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            result.success = false;
            result.error   = "Cannot open file for writing: " + path;
            result.output  = {{"path", path}, {"bytes_written", 0}};
        } else {
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (file.fail()) {
                result.success = false;
                result.error   = "Write failed for: " + path;
                result.output  = {{"path", path}, {"bytes_written", 0}};
            } else {
                file.close();
                result.success = true;
                result.output  = {
                    {"path", path},
                    {"bytes_written", static_cast<int>(content.size())}
                };
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

void registerWriteAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "WriteAction",
        "Write content to a file (truncating it), creating parent directories as needed.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"path", "content"}},
            {"properties", {
                {"path",    {{"type", "string"}, {"description", "Destination file path."}}},
                {"content", {{"type", "string"}, {"description", "Text content to write."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<WriteAction>(std::move(id), "WriteAction", std::move(inputs));
    });
}

} // namespace agent
