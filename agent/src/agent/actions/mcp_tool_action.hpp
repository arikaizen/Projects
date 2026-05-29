#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Stub wrapper for MCP (Model Context Protocol) server tool calls.
// In a full implementation this would look up the MCP connection registered
// in AgentManager and dispatch the call using per-request correlation IDs.
// Concurrent calls to the same MCP server are safe because each call carries
// a unique request ID; the server demultiplexes them.
//
// Actual MCP connection lives in AgentManager::m_mcp_servers.
class MCPToolAction : public Action {
public:
    MCPToolAction(std::string id,
                  std::string tool_name,
                  std::string server_name,
                  nlohmann::json inputs = {});

    WorkResult execute(AgentContext& ctx) override;

private:
    std::string m_tool_name;
    std::string m_server_name;
};

void registerMCPToolAction(WorkFactory& factory);

} // namespace agent
