# MCPToolAction

`src/agent/actions/mcp_tool_action.hpp` · `src/agent/actions/mcp_tool_action.cpp`
**Factory name:** `MCPToolAction` · **Kind:** Action

---

## Purpose

Dispatches a call to a tool on a connected MCP (Model Context Protocol) server. **Stub** as shipped — it prints a diagnostic and returns a placeholder; a full implementation would look up the connection registered via `AgentManager::connectMCP` and dispatch with a unique correlation id.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `tool_name` | string | **yes** | Name of the MCP tool to call |
| `server_name` | string | **yes** | Name of the MCP server |
| `arguments` | object | no | Tool-specific arguments |

## Construction

```cpp
MCPToolAction(std::string id, std::string tool_name,
              std::string server_name, nlohmann::json inputs = {});
```

## Output

A result object echoing `tool`/`server` plus a `note` that the call is a stub until MCP is wired up.

## Status

⚠️ Stub — the MCP connection registry lives in `AgentManager::m_mcp_servers` but is not yet used for live dispatch.

## Related

- [Actions overview](actions.md) · [AgentManager](agent_manager.md) — `connectMCP` / `disconnectMCP` / `listMCPServers`
- [WorkItem](work_item.md)
