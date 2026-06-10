#include "mcp_tool_action.hpp"
#include "agent/agent_context.hpp"
#include "agent/agent_manager.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

#ifdef AGENT_HAS_MCP_HTTP
#  include <httplib.h>
#endif

namespace agent {

namespace {

std::string makeRequestId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    return "mcp-" + std::to_string(rng());
}

} // anonymous namespace

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
        nlohmann::json arguments = resolved_inputs.value("arguments", nlohmann::json::object());

        // Look up the registered MCP server configuration.
        AgentManager* mgr = ctx.manager();
        if (!mgr) throw std::runtime_error("MCPToolAction: AgentManager not available in context");

        nlohmann::json servers = mgr->listMCPServers();
        if (!servers.contains(m_server_name)) {
            throw std::runtime_error("MCPToolAction: server '" + m_server_name + "' not registered — call AgentManager::connectMCP() first");
        }

        auto& cfg = servers[m_server_name];
        std::string url          = cfg.value("url", std::string{});
        std::string bearer_token = cfg.value("bearer_token", std::string{});
        std::string transport    = cfg.value("transport", std::string{"http"});

        std::string req_id = makeRequestId();

#ifdef AGENT_HAS_MCP_HTTP
        if (transport == "http") {
            if (url.empty())
                throw std::runtime_error("MCPToolAction: server '" + m_server_name + "' has no url");
            nlohmann::json rpc_result = callHttp(url, bearer_token, arguments, req_id);
            result.success = true;
            result.output  = rpc_result;
        } else {
            // stdio path — not implemented in this TU; handled by AgentManager pipe
            throw std::runtime_error("MCPToolAction: stdio transport requires AgentManager stdio pipe (not yet wired)");
        }
#else
        // No HTTP library compiled in — log intent and return informative result
        (void)url; (void)bearer_token; (void)transport; (void)req_id;
        std::cerr << "[ACTION:" << name << "] MCP HTTP not compiled in "
                  << "(rebuild with -DAGENT_ENABLE_MCP_HTTP=ON). "
                  << "Would call server=\"" << m_server_name
                  << "\" tool=\"" << m_tool_name << "\"\n";
        result.success = true;
        result.output  = {
            {"note",      "Rebuild with -DAGENT_ENABLE_MCP_HTTP=ON for live calls."},
            {"tool",      m_tool_name},
            {"server",    m_server_name},
            {"arguments", arguments},
        };
#endif

    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

#ifdef AGENT_HAS_MCP_HTTP
nlohmann::json MCPToolAction::callHttp(const std::string& url,
                                        const std::string& bearer_token,
                                        const nlohmann::json& arguments,
                                        const std::string& request_id)
{
    // Build a JSON-RPC 2.0 tools/call request (step ③)
    nlohmann::json rpc_request = {
        {"jsonrpc", "2.0"},
        {"id",      request_id},
        {"method",  "tools/call"},
        {"params",  {
            {"name",      m_tool_name},
            {"arguments", arguments},
        }},
    };

    // Parse the base URL to extract host, port, and path prefix
    // Expected format: http[s]://host[:port][/prefix]
    std::string scheme, host, path_prefix;
    int port = 80;

    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos)
        throw std::runtime_error("MCPToolAction: malformed URL: " + url);

    scheme = url.substr(0, scheme_end);
    std::string rest = url.substr(scheme_end + 3);
    auto slash_pos = rest.find('/');
    std::string host_port = (slash_pos == std::string::npos) ? rest : rest.substr(0, slash_pos);
    path_prefix = (slash_pos == std::string::npos) ? "" : rest.substr(slash_pos);

    auto colon_pos = host_port.rfind(':');
    if (colon_pos != std::string::npos) {
        host = host_port.substr(0, colon_pos);
        port = std::stoi(host_port.substr(colon_pos + 1));
    } else {
        host = host_port;
        port = (scheme == "https") ? 443 : 80;
    }

    std::string endpoint = path_prefix + "/mcp/v1";
    std::string body = rpc_request.dump();

    httplib::Headers headers;
    if (!bearer_token.empty())
        headers.emplace("Authorization", "Bearer " + bearer_token);
    headers.emplace("Content-Type", "application/json");

    std::unique_ptr<httplib::Client> cli;
    if (scheme == "https") {
        cli = std::make_unique<httplib::SSLClient>(host, port);
    } else {
        cli = std::make_unique<httplib::Client>(host, port);
    }
    cli->set_connection_timeout(5);
    cli->set_read_timeout(30);

    auto res = cli->Post(endpoint.c_str(), headers, body, "application/json");
    if (!res) {
        auto err = res.error();
        throw std::runtime_error("MCPToolAction: HTTP request failed: " +
                                 httplib::to_string(err));
    }
    if (res->status < 200 || res->status >= 300) {
        throw std::runtime_error("MCPToolAction: HTTP " + std::to_string(res->status) +
                                 " — " + res->body);
    }

    auto response_json = nlohmann::json::parse(res->body);
    if (response_json.contains("error")) {
        auto& err = response_json["error"];
        throw std::runtime_error("MCPToolAction: JSON-RPC error " +
                                 std::to_string(err.value("code", 0)) +
                                 ": " + err.value("message", "unknown"));
    }

    return response_json.value("result", nlohmann::json{});
}
#endif // AGENT_HAS_MCP_HTTP

void registerMCPToolAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "MCPToolAction",
        "Call a tool on a connected MCP server via JSON-RPC 2.0 over Streamable HTTP or stdio.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"tool_name", "server_name"}},
            {"properties", {
                {"tool_name",   {{"type", "string"}, {"description", "Name of the MCP tool to call."}}},
                {"server_name", {{"type", "string"}, {"description", "Name of the registered MCP server."}}},
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
