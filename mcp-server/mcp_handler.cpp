/**
 * @file mcp_handler.cpp
 * @brief MCP method dispatch and tool execution implementation.
 */

#include "mcp_handler.h"
#include "tool_registry.h"
#include "db_client.h"
#include "json_rpc.h"

#include <stdexcept>
#include <iostream>   // std::cerr for debug logging

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

McpHandler::McpHandler(ToolRegistry& registry, DbClient& db)
    : m_registry(registry), m_db(db) {}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * Wraps a text string in the MCP tools/call content array format:
 * @code
 *   {"content":[{"type":"text","text":"<escaped text>"}]}
 *   {"content":[{"type":"text","text":"..."}],"isError":true}
 * @endcode
 *
 * isError:true signals to the LLM that the tool execution failed so it can
 * reason about the error rather than treat the text as a successful result.
 */
std::string McpHandler::makeContentResult(const std::string& text, bool isError) {
    std::string result = "{\"content\":[{\"type\":\"text\",\"text\":\"";
    result += json_rpc::jsonEscape(text);
    result += "\"}]";
    if (isError) result += ",\"isError\":true";
    result += "}";
    return result;
}

/**
 * Builds a raw JSON error object for McpResponse::error.
 * {"code": <code>, "message": "<escaped message>"}
 */
std::string McpHandler::makeErrorObject(int code, const std::string& message) {
    std::string obj = "{\"code\":";
    obj += std::to_string(code);
    obj += ",\"message\":\"";
    obj += json_rpc::jsonEscape(message);
    obj += "\"}";
    return obj;
}

// ---------------------------------------------------------------------------
// Public dispatch
// ---------------------------------------------------------------------------

McpResponse McpHandler::handle(const McpRequest& request) {
    try {
        if (request.method == "initialize")
            return handleInitialize(request);

        if (request.method == "notifications/initialized") {
            // Client notifying us that its side of the handshake is complete.
            // This is a notification — no response is expected.  Return an empty
            // response; McpServer will suppress it because req.id == "null".
            McpResponse res;
            res.jsonrpc = "2.0";
            res.id      = request.id;
            res.result  = "{}";
            return res;
        }

        if (request.method == "tools/list")
            return handleToolsList(request);

        if (request.method == "tools/call")
            return handleToolsCall(request);

        if (request.method == "ping")
            return handlePing(request);

        // Unknown method — JSON-RPC standard error -32601.
        McpResponse res;
        res.jsonrpc = "2.0";
        res.id      = request.id;
        res.error   = makeErrorObject(-32601,
                          "Method not found: " + request.method);
        return res;

    } catch (const std::exception& e) {
        // Defensive catch — internal error should not crash the server.
        std::cerr << "[mcp_handler] internal error: " << e.what() << "\n";
        McpResponse res;
        res.jsonrpc = "2.0";
        res.id      = request.id;
        res.error   = makeErrorObject(-32603, std::string("Internal error: ") + e.what());
        return res;
    }
}

// ---------------------------------------------------------------------------
// handleInitialize
// ---------------------------------------------------------------------------

McpResponse McpHandler::handleInitialize(const McpRequest& req) {
    // Return the server's capabilities.  We advertise only "tools" since that
    // is the only MCP capability this server implements.
    //
    // The protocolVersion must match a version supported by the connecting host.
    // "2024-11-05" is the current stable MCP specification version.
    static const std::string capabilities =
        "{"
        "\"protocolVersion\":\"2024-11-05\","
        "\"capabilities\":{\"tools\":{}},"
        "\"serverInfo\":{\"name\":\"graph-db-mcp\",\"version\":\"1.0.0\"}"
        "}";

    McpResponse res;
    res.jsonrpc = "2.0";
    res.id      = req.id;
    res.result  = capabilities;

    std::cerr << "[mcp_handler] initialize complete, capabilities sent\n";
    return res;
}

// ---------------------------------------------------------------------------
// handleToolsList
// ---------------------------------------------------------------------------

McpResponse McpHandler::handleToolsList(const McpRequest& req) {
    McpResponse res;
    res.jsonrpc = "2.0";
    res.id      = req.id;
    res.result  = m_registry.listToolsJson();
    return res;
}

// ---------------------------------------------------------------------------
// handleToolsCall
// ---------------------------------------------------------------------------

McpResponse McpHandler::handleToolsCall(const McpRequest& req) {
    McpResponse res;
    res.jsonrpc = "2.0";
    res.id      = req.id;

    // ---- Step 1: parse the ToolCall from the params field ----
    ToolCall call = json_rpc::parseToolCall(req.params);
    if (call.name.empty()) {
        res.error = makeErrorObject(-32602,
            "Invalid params: could not parse tool name from params");
        return res;
    }

    // ---- Step 2: validate against the registry ----
    std::string validationErr;
    if (!m_registry.validate(call, validationErr)) {
        res.error = makeErrorObject(-32602, validationErr);
        return res;
    }

    // ---- Step 3: translate to graph DB wire command ----
    std::string dbCommand = DbClient::translateToDbCommand(call);
    if (dbCommand.empty()) {
        res.error = makeErrorObject(-32602,
            "No DB command mapping for tool: " + call.name);
        return res;
    }

    std::cerr << "[mcp_handler] DB → " << dbCommand << "\n";

    // ---- Step 4: send to graph DB and read response ----
    std::string dbResponse = m_db.send(dbCommand);

    std::cerr << "[mcp_handler] DB ← " << dbResponse << "\n";

    // ---- Step 5: parse DB response and wrap in MCP content format ----
    if (dbResponse.size() >= 3 && dbResponse.substr(0, 3) == "OK ") {
        // Successful response — extract the JSON payload after "OK ".
        std::string payload = dbResponse.substr(3);
        res.result = makeContentResult(payload, false);

    } else if (dbResponse.size() >= 6 && dbResponse.substr(0, 6) == "ERROR ") {
        // The graph DB returned an application-level error.
        // Return as content with isError:true so the LLM sees the message.
        std::string errMsg = dbResponse.substr(6);
        res.result = makeContentResult(errMsg, true);

    } else {
        // Unexpected format — treat as internal error.
        res.result = makeContentResult(
            "Unexpected graph DB response: " + dbResponse, true);
    }

    return res;
}

// ---------------------------------------------------------------------------
// handlePing
// ---------------------------------------------------------------------------

McpResponse McpHandler::handlePing(const McpRequest& req) {
    McpResponse res;
    res.jsonrpc = "2.0";
    res.id      = req.id;
    res.result  = "{}";
    return res;
}
