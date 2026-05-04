/**
 * @file mcp_handler.h
 * @brief MCP method dispatcher — routes JSON-RPC methods to their implementations.
 *
 * McpHandler is the business-logic layer of the MCP server.  It sits between
 * McpServer (I/O loop) and the graph DB (DbClient) and is responsible for:
 *
 *   - Dispatching each JSON-RPC method to the correct handler function.
 *   - Performing tool validation before forwarding calls to the graph DB.
 *   - Translating graph DB responses into MCP tool-call result format.
 *   - Wrapping all exceptions so the server never crashes on bad input.
 *
 * Supported methods
 * -----------------
 *   initialize              — MCP handshake; returns server capabilities.
 *   notifications/initialized — Client notifying handshake complete (notification,
 *                              no response sent).
 *   tools/list              — Returns all registered tool schemas.
 *   tools/call              — Validates, translates, and forwards a tool invocation.
 *   ping                    — Heartbeat; returns empty result {}.
 *
 * Error taxonomy
 * --------------
 *   Protocol errors (JSON-RPC error field):
 *     -32601  Unknown method
 *     -32602  Invalid params (bad tool name or missing required argument)
 *     -32603  Internal error (unexpected exception)
 *
 *   Tool-level errors (result field with isError:true):
 *     Graph DB returned "ERROR ..." — passed to the LLM as content so it
 *     can reason about the error rather than receive a protocol failure.
 */

#ifndef MCP_HANDLER_H
#define MCP_HANDLER_H

#include "mcp_types.h"

// Forward declarations — full definitions only needed in .cpp.
class ToolRegistry;
class DbClient;

/**
 * @brief Dispatches MCP JSON-RPC requests to their handler implementations.
 */
class McpHandler {
public:
    /**
     * @brief Constructs the handler with its two dependencies.
     *
     * @param registry  The tool schema registry used for discovery and validation.
     *                  Must remain valid for the lifetime of this McpHandler.
     * @param db        The TCP client connected to the graph DB server.
     *                  Must remain valid for the lifetime of this McpHandler.
     */
    McpHandler(ToolRegistry& registry, DbClient& db);

    /**
     * @brief Dispatches one JSON-RPC request and returns a response.
     *
     * All exceptions are caught and converted to JSON-RPC -32603 internal
     * error responses so the server loop is never interrupted by bad input.
     *
     * @param request  The parsed JSON-RPC request to handle.
     * @return         A populated McpResponse ready for serialization.
     */
    McpResponse handle(const McpRequest& request);

private:
    ToolRegistry& m_registry; ///< Tool schema source for list and validation
    DbClient&     m_db;       ///< TCP connection to the graph DB

    // ------------------------------------------------------------------
    // Per-method handlers
    // ------------------------------------------------------------------

    /** Handles "initialize" — returns server capabilities object. */
    McpResponse handleInitialize(const McpRequest& req);

    /** Handles "tools/list" — returns all registered tool schemas as JSON. */
    McpResponse handleToolsList(const McpRequest& req);

    /**
     * Handles "tools/call":
     *   1. Parses the ToolCall from req.params.
     *   2. Validates required arguments via ToolRegistry.
     *   3. Translates to a graph DB command via DbClient::translateToDbCommand.
     *   4. Sends to the graph DB and reads the response.
     *   5. Wraps the response in MCP content format.
     */
    McpResponse handleToolsCall(const McpRequest& req);

    /** Handles "ping" — returns empty result {}. */
    McpResponse handlePing(const McpRequest& req);

    // ------------------------------------------------------------------
    // Response building helpers
    // ------------------------------------------------------------------

    /**
     * Wraps a plain text string in the MCP tools/call content array format.
     *
     * Success:  {"content":[{"type":"text","text":"<text>"}]}
     * Failure:  {"content":[{"type":"text","text":"<text>"}],"isError":true}
     *
     * isError:true tells the LLM the tool failed so it can respond
     * appropriately rather than treating the text as a successful result.
     *
     * @param text     The message to deliver to the LLM.
     * @param isError  true if this represents a tool execution failure.
     * @return         Raw JSON string for use as McpResponse::result.
     */
    std::string makeContentResult(const std::string& text, bool isError);

    /**
     * Builds a raw JSON error object for use in McpResponse::error.
     *
     * @param code     JSON-RPC error code (e.g. -32602).
     * @param message  Human-readable error description.
     * @return         {"code":<code>,"message":"<escaped message>"}
     */
    std::string makeErrorObject(int code, const std::string& message);
};

#endif // MCP_HANDLER_H
