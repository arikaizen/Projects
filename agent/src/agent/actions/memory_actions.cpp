#include "memory_actions.hpp"
#include "agent/agent_context.hpp"
#include "agent/memory_backend.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

// ── MemoryWriteAction ────────────────────────────────────────────────────────

WorkResult MemoryWriteAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto mem_id   = resolved.at("id").get<std::string>();
        auto content  = resolved.at("content").get<std::string>();
        auto metadata = resolved.contains("metadata") && resolved["metadata"].is_object()
                            ? resolved["metadata"]
                            : nlohmann::json::object();

        std::cerr << "[ACTION:" << name << "] memory write id=\"" << mem_id << "\"\n";

        ctx.memory().write(mem_id, content, metadata);

        result.success = true;
        result.output  = {{"id", mem_id}, {"written", true}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── MemoryReadAction ─────────────────────────────────────────────────────────

WorkResult MemoryReadAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto query    = resolved.at("query").get<std::string>();
        int  top_k    = resolved.value("top_k", 5);

        std::cerr << "[ACTION:" << name << "] memory search query=\"" << query
                  << "\" top_k=" << top_k << "\n";

        auto entries = ctx.memory().search(query, top_k);

        nlohmann::json entries_json = nlohmann::json::array();
        for (const auto& e : entries) {
            entries_json.push_back({
                {"id",      e.id},
                {"content", e.content},
                {"score",   e.score}
            });
        }

        result.success = true;
        result.output  = {{"entries", entries_json}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── MemoryListAction ─────────────────────────────────────────────────────────

WorkResult MemoryListAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto filter   = resolved.value("filter", std::string(""));

        std::cerr << "[ACTION:" << name << "] memory list filter=\"" << filter << "\"\n";

        auto entries = ctx.memory().list(filter);

        nlohmann::json entries_json = nlohmann::json::array();
        for (const auto& e : entries) {
            entries_json.push_back({
                {"id",      e.id},
                {"content", e.content}
            });
        }

        result.success = true;
        result.output  = {{"entries", entries_json}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

// ── Registration ─────────────────────────────────────────────────────────────

void registerMemoryActions(WorkFactory& factory) {
    // MemoryWriteAction
    factory.registerItem(
        WorkItemSpec{
            "MemoryWriteAction",
            "Write an entry to the agent's memory backend.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"required", {"id", "content"}},
                {"properties", {
                    {"id",       {{"type", "string"}, {"description", "Unique memory entry ID."}}},
                    {"content",  {{"type", "string"}, {"description", "Content to store."}}},
                    {"metadata", {{"type", "object"}, {"description", "Optional metadata."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<MemoryWriteAction>(std::move(id), "MemoryWriteAction", std::move(inputs));
        }
    );

    // MemoryReadAction
    factory.registerItem(
        WorkItemSpec{
            "MemoryReadAction",
            "Search the agent's memory backend for entries matching a query.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"required", {"query"}},
                {"properties", {
                    {"query", {{"type", "string"}, {"description", "Search query."}}},
                    {"top_k", {{"type", "integer"}, {"description", "Max results to return (default 5)."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<MemoryReadAction>(std::move(id), "MemoryReadAction", std::move(inputs));
        }
    );

    // MemoryListAction
    factory.registerItem(
        WorkItemSpec{
            "MemoryListAction",
            "List all memory entries, optionally filtered by a substring.",
            WorkItem::Kind::Action,
            {
                {"type", "object"},
                {"properties", {
                    {"filter", {{"type", "string"}, {"description", "Optional substring filter."}}},
                }}
            }
        },
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<MemoryListAction>(std::move(id), "MemoryListAction", std::move(inputs));
        }
    );
}

} // namespace agent
