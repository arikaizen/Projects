#include "mcp_tool_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

MCPToolAction::MCPToolAction(std::string id,
                             std::string tool_name,
                             std::string server_name,
                             nlohmann::json inputs)
    : Action(std::move(id), tool_name, std::move(inputs))
    , m_tool_name(std::move(tool_name))
    , m_server_name(std::move(server_name)) {}

WorkResult MCPToolAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved_inputs = ctx.resolveReferences(inputs);

        std::cerr << "[ACTION:" << name << "] MCP tool call (stub): server=\""
                  << m_server_name << "\" tool=\"" << m_tool_name << "\"\n";

        // Stub: a real implementation would:
        //   1. Retrieve the MCP connection from AgentManager via ctx.manager().
        //   2. Serialize `resolved_inputs` as the tool call arguments.
        //   3. Assign a unique request ID and send the JSON-RPC request.
        //   4. Await the response with a timeout.
        //   5. Deserialize and return the result.

        result.success = true;
        result.output  = {
            {"note",    "MCP tool call stub — wire up AgentManager::connectMCP() to enable real calls."},
            {"tool",    m_tool_name},
            {"server",  m_server_name},
            {"inputs",  resolved_inputs}
        };
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerMCPToolAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "MCPToolAction",
        "Call a tool on a connected MCP server. (Stub: requires MCP connection to be configured.)",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"tool_name", "server_name"}},
            {"properties", {
                {"tool_name",   {{"type", "string"}, {"description", "Name of the MCP tool to call."}}},
                {"server_name", {{"type", "string"}, {"description", "Name of the MCP server."}}},
                {"arguments",   {{"type", "object"}, {"description", "Tool-specific arguments."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        std::string tool_name   = inputs.value("tool_name",   std::string("unknown_tool"));
        std::string server_name = inputs.value("server_name", std::string("unknown_server"));
        return std::make_unique<MCPToolAction>(
            std::move(id), std::move(tool_name), std::move(server_name), std::move(inputs));
    });
}

} // namespace agent
