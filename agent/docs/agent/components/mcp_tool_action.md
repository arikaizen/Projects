# MCPToolAction

`src/agent/actions/mcp_tool_action.hpp` · `src/agent/actions/mcp_tool_action.cpp`

## Overview

`MCPToolAction` is a **stub** that will call a tool on a connected MCP (Model Context Protocol) server. Currently it resolves inputs, logs the call, and returns a stub response with a configuration note.

## Factory Registration

```
name:  "MCPToolAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Description |
|---|---|---|---|
| `tool_name` | string | Yes | Name of the MCP tool to call |
| `server_name` | string | Yes | Name of the connected MCP server |
| `arguments` | object | No | Tool-specific arguments |

## Current Behaviour (stub)

Returns:

```json
{
  "note": "MCP tool call stub — wire up AgentManager::connectMCP() to enable real calls.",
  "tool": "<tool_name>",
  "server": "<server_name>",
  "inputs": { ... }
}
```

`success` is always `true`.

## Construction

Unlike other actions, `MCPToolAction` is constructed with extra parameters extracted from `inputs`:

```cpp
MCPToolAction(std::string id, std::string tool_name,
              std::string server_name, nlohmann::json inputs);
```

The `WorkFactory` `CreateFn` extracts `tool_name` and `server_name` from `inputs` before construction.

## Implementing a Real Backend

To enable real MCP calls, replace the stub body in `mcp_tool_action.cpp` with:

1. Retrieve the MCP connection from `AgentManager` via `ctx.manager()`.
2. Serialize `resolved_inputs` as JSON-RPC tool call arguments.
3. Assign a unique request ID and send the request.
4. Await the response with a timeout.
5. Deserialize and return the result.

## Related Components

- [`Action`](action.md) — base class
- [`AgentManager`](agent_manager.md) — `connectMCP`, `disconnectMCP`, `listMCPServers`
- [`C ABI`](c_api.md) — `am_connect_mcp`, `am_disconnect_mcp`, `am_list_mcp_servers`
