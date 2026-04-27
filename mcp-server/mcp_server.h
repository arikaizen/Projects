/**
 * @file mcp_server.h
 * @brief stdin/stdout event loop for the MCP JSON-RPC protocol.
 *
 * McpServer is the outermost layer of the MCP server.  It owns the I/O loop
 * and enforces the MCP framing contract:
 *
 *   Framing contract
 *   ----------------
 *   - Input  (stdin) : one complete JSON object per line, '\n'-terminated.
 *   - Output (stdout): one complete JSON object per line, '\n'-terminated,
 *                      immediately flushed after every write.
 *
 * The flush after every write is MANDATORY.  The LLM host (Claude Desktop,
 * Claude Code) reads from stdout using a line-buffered or blocking read; if
 * the server's stdout buffer is not flushed, the host will block indefinitely
 * waiting for data that is sitting in the C++ I/O buffer.
 *
 * stdout purity
 * -------------
 * stdout is reserved exclusively for MCP JSON-RPC messages.  A single stray
 * byte (debug print, warning, etc.) will corrupt the protocol stream and cause
 * the host to misparse all subsequent messages.  All logging goes to stderr.
 *
 * Notification handling
 * ---------------------
 * JSON-RPC notifications have no "id" field (stored as "null" by the parser).
 * Notifications must NOT receive a response.  McpServer detects them and
 * suppresses the response write while still passing the message to McpHandler
 * for processing.
 *
 * Shutdown
 * --------
 * The loop exits naturally when stdin is closed (std::getline returns false).
 * The caller is responsible for cleanup (DbClient::disconnect, etc.).
 */

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

// Forward declaration — full McpHandler definition only needed in .cpp.
class McpHandler;

/**
 * @brief Reads MCP requests from stdin, dispatches to McpHandler, and writes
 *        responses to stdout.
 */
class McpServer {
public:
    /**
     * @brief Constructs the server bound to a handler.
     *
     * @param handler  The request dispatcher.  Must remain valid for the
     *                 lifetime of this McpServer.
     */
    explicit McpServer(McpHandler& handler);

    /**
     * @brief Enters the blocking event loop — does not return until stdin closes.
     *
     * Processing per line:
     *   1. Read one line from std::cin with std::getline.
     *   2. Skip blank lines.
     *   3. Parse the line with json_rpc::parseRequest().
     *   4. If parsing failed (empty method and no id), write a -32700 error.
     *   5. Pass the request to McpHandler::handle().
     *   6. If the request was a notification (id == "null"), suppress output.
     *   7. Otherwise serialize the response and write + flush to stdout.
     */
    void run();

private:
    McpHandler& m_handler; ///< Delegates all request processing here
};

#endif // MCP_SERVER_H
