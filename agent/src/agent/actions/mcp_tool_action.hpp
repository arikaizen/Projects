#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Transport mode for an MCP server connection.
enum class MCPTransport {
    Http,   // Streamable HTTP — Bearer token required (remote server)
    Stdio,  // stdin/stdout subprocess — no network auth (local server)
};

// Calls a tool on a connected MCP server using JSON-RPC 2.0.
//
// HTTP transport: sends POST <url>/mcp/v1 with Authorization: Bearer <token>.
// Stdio transport: writes to the subprocess stdin managed by AgentManager.
//
// Concurrent calls to the same server are safe: each carries a unique
// request ID so the server can demultiplex them.
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

#ifdef AGENT_HAS_MCP_HTTP
    // Performs the HTTP POST and returns the parsed JSON-RPC result or throws.
    nlohmann::json callHttp(const std::string& url,
                            const std::string& bearer_token,
                            const nlohmann::json& arguments,
                            const std::string& request_id);
#endif
};

void registerMCPToolAction(WorkFactory& factory);

} // namespace agent
