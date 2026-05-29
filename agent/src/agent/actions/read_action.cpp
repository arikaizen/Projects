#include "read_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult ReadAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto path     = resolved.at("path").get<std::string>();

        int offset = 0; // 1-based; 0 means start from the beginning
        int limit  = -1; // -1 means no limit

        if (resolved.contains("offset") && !resolved["offset"].is_null()) {
            offset = resolved["offset"].get<int>();
        }
        if (resolved.contains("limit") && !resolved["limit"].is_null()) {
            limit = resolved["limit"].get<int>();
        }

        std::cerr << "[ACTION:" << name << "] reading file: " << path;
        if (offset > 0) std::cerr << " (offset=" << offset << ")";
        if (limit >= 0) std::cerr << " (limit=" << limit << ")";
        std::cerr << "\n";

        std::ifstream file(path);
        if (!file.is_open()) {
            result.success = false;
            result.error   = "Cannot open file: " + path;
            result.output  = {{"content", ""}, {"path", path}, {"lines", 0}};
        } else {
            std::string content;
            int         total_lines = 0;

            if (offset <= 1 && limit < 0) {
                // Fast path: read whole file.
                std::ostringstream ss;
                ss << file.rdbuf();
                content = ss.str();
                // Count lines.
                for (char c : content) {
                    if (c == '\n') ++total_lines;
                }
                if (!content.empty() && content.back() != '\n') ++total_lines;
            } else {
                // Line-by-line with offset / limit.
                std::string line;
                int         current_line = 0;
                int         lines_kept   = 0;
                // Effective start: offset is 1-based; 0 or 1 both mean first line.
                int start = (offset <= 1) ? 1 : offset;
                while (std::getline(file, line)) {
                    ++current_line;
                    ++total_lines;
                    if (current_line < start) continue;
                    if (limit >= 0 && lines_kept >= limit) {
                        // Still count remaining lines.
                        continue;
                    }
                    content += line + "\n";
                    ++lines_kept;
                }
            }

            result.success = true;
            result.output  = {
                {"content", content},
                {"path", path},
                {"lines", total_lines}
            };
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerReadAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "ReadAction",
        "Read a file from disk. Optionally select a line range with offset (1-based) and limit.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"path"}},
            {"properties", {
                {"path",   {{"type", "string"}, {"description", "Absolute or relative file path."}}},
                {"offset", {{"type", "integer"}, {"description", "1-based start line (default: 1)."}}},
                {"limit",  {{"type", "integer"}, {"description", "Max number of lines to return."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<ReadAction>(std::move(id), "ReadAction", std::move(inputs));
    });
}

} // namespace agent
