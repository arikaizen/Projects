/**
 * @file mcp_handler.h
 * @brief Routes MCP method calls to the appropriate graph DB operations.
 *
 * McpHandler is the central dispatch point for all JSON-RPC requests.  It
 * receives fully-parsed McpRequest objects from McpServer, routes them to
 * private handler methods, and returns McpResponse objects ready for
 * serialization.
 *
 * Routing table
 * -------------
 *   "initialize"              → handleInitialize  — return server capabilities
 *   "notifications/initialized" → (notification, no response needed)
 *   "tools/list"              → handleToolsList   — return all tool schemas
 *   "tools/call"              → handleToolsCall   — validate, send to DB, wrap result
 *   "ping"                    → handlePing        — return empty result
 *   anything else             → JSON-RPC error -32601 (method not found)
 *
 * Tool call result format
 * -----------------------
 * Successful tool calls return an MCP content array in the result field:
 * @code
 *   {"content":[{"type":"text","text":"<db json response>"}]}
 * @endcode
 *
 * Tool errors (ERROR responses from the graph DB, or validation failures)
 * are returned as content with isError:true rather than as JSON-RPC errors,
 * so the LLM can see the error message and potentially recover.  Only
 * protocol-level failures (unknown method, malformed params) use JSON-RPC
 * error codes.
 */

#ifndef MCP_HANDLER_H
#define MCP_HANDLER_H

#include "mcp_types.h"
#include <string>

// Forward declarations — full definitions are only needed in mcp_handler.cpp.
class ToolRegistry;
class DbClient;

/**
 * @brief Dispatches McpRequest objects to the appropriate handler and returns
 *        McpResponse objects.
 */
class McpHandler {
public:
    /**
     * @brief Constructs the handler bound to a ToolRegistry and DbClient.
     *
     * Both objects must remain valid for the lifetime of this handler.
     *
     * @param registry  Tool schema store used for discovery and validation.
     * @param db        TCP client used to forward commands to the graph DB.
     */
    McpHandler(ToolRegistry& registry, DbClient& db);

    /**
     * @brief Processes one MCP request and returns the response.
     *
     * This method never throws — all exceptions are caught internally and
     * converted into JSON-RPC error responses.
     *
     * For notification messages (req.id == "null") the handler still processes
     * the message for side effects, but the returned McpResponse will have an
     * empty result — the caller (McpServer) is responsible for not sending a
     * response for notifications.
     *
     * @param request  The parsed request to handle.
     * @return         A fully populated McpResponse with either result or error set.
     */
    McpResponse handle(const McpRequest& request);

private:
    ToolRegistry& m_registry; ///< Tool schema source
    DbClient&     m_db;       ///< TCP connection to the graph DB

    /**
     * @brief Handles "initialize" — returns server metadata and capabilities.
     *
     * The response declares this server's name, version, and supported
     * capability set (tools only).  The LLM host uses this to determine which
     * MCP features are available.
     *
     * @param req  The initialize request (params contain client info, ignored).
     * @return     McpResponse with the server capabilities JSON as result.
     */
    McpResponse handleInitialize(const McpRequest& req);

    /**
     * @brief Handles "tools/list" — returns all tool schemas.
     *
     * Delegates to ToolRegistry::listToolsJson() and wraps the result in a
     * standard McpResponse.
     *
     * @param req  The tools/list request (no params needed).
     * @return     McpResponse with the tools array JSON as result.
     */
    McpResponse handleToolsList(const McpRequest& req);

    /**
     * @brief Handles "tools/call" — validates, translates, executes, and wraps.
     *
     * Steps:
     *   1. Parse the ToolCall from req.params using json_rpc::parseToolCall().
     *   2. Validate against the registry (tool exists, required args present).
     *   3. Translate to a graph DB command via DbClient::translateToDbCommand().
     *   4. Send the command to the graph DB via DbClient::send().
     *   5. Parse the "OK ..." or "ERROR ..." response.
     *   6. Wrap in an MCP content response.
     *
     * @param req  The tools/call request containing tool name and arguments.
     * @return     McpResponse with content result or error.
     */
    McpResponse handleToolsCall(const McpRequest& req);

    /**
     * @brief Handles "ping" — returns an empty result to confirm liveness.
     *
     * @param req  The ping request.
     * @return     McpResponse with empty result "{}".
     */
    McpResponse handlePing(const McpRequest& req);

    /**
     * @brief Builds the MCP content response for a successful tool call.
     *
     * @param text     The text to embed in the content array (DB JSON output).
     * @param isError  Whether to set isError:true (for DB-level errors).
     * @return         Raw JSON result string for McpResponse::result.
     */
    static std::string makeContentResult(const std::string& text, bool isError);

    /**
     * @brief Builds a JSON-RPC error object string for McpResponse::error.
     *
     * @param code     JSON-RPC error code.
     * @param message  Human-readable error message (will be JSON-escaped).
     * @return         Raw JSON error object {"code":N,"message":"..."}.
     */
    static std::string makeErrorObject(int code, const std::string& message);
};

#endif // MCP_HANDLER_H
