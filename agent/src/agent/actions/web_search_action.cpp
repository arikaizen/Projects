#include "web_search_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult WebSearchAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved   = ctx.resolveReferences(inputs);
        auto query      = resolved.at("query").get<std::string>();
        int  num_results = resolved.value("num_results", 5);

        std::cerr << "[ACTION:" << name << "] web search (stub): \"" << query
                  << "\" num_results=" << num_results << "\n";

        result.success = true;
        result.output  = {
            {"results", nlohmann::json::array()},
            {"query",   query},
            {"note",    "WebSearch requires API key configuration. "
                        "Set SEARCH_API_KEY in the environment and configure the search "
                        "provider in AgentConfig::extra[\"search_provider\"]."}
        };
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerWebSearchAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "WebSearchAction",
        "Search the web for a query. Returns a stub response until a search API key is configured.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"query"}},
            {"properties", {
                {"query",       {{"type", "string"}, {"description", "Search query."}}},
                {"num_results", {{"type", "integer"}, {"description", "Desired number of results (default 5)."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<WebSearchAction>(std::move(id), "WebSearchAction", std::move(inputs));
    });
}

} // namespace agent
