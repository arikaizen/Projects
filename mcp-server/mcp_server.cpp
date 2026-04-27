/**
 * @file mcp_server.cpp
 * @brief Implementation of the MCP stdin/stdout event loop.
 */

#include "mcp_server.h"
#include "mcp_handler.h"
#include "json_rpc.h"
#include "mcp_types.h"

#include <iostream>   // std::cin, std::cout, std::cerr
#include <string>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

McpServer::McpServer(McpHandler& handler)
    : m_handler(handler) {}

// ---------------------------------------------------------------------------
// run — the main event loop
// ---------------------------------------------------------------------------

void McpServer::run() {
    std::string line;

    // std::getline returns false (and breaks) when stdin reaches EOF, which is
    // the normal shutdown signal sent by the MCP host when it terminates the
    // child process.
    while (std::getline(std::cin, line)) {

        // ---- Skip blank lines ----
        // Some MCP hosts send a trailing blank line as a heartbeat or separator.
        if (line.empty() || (line.size() == 1 && line[0] == '\r')) continue;

        // Strip a stray '\r' left by Windows-style CRLF line endings.
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::cerr << "[mcp_server] recv: " << line << "\n";

        // ---- Step 1: parse the JSON-RPC request ----
        McpRequest req = json_rpc::parseRequest(line);

        // ---- Step 2: detect fatal parse errors ----
        // If the method is empty AND the id is "null", the line was not valid
        // JSON-RPC at all.  Return a parse-error response with null id.
        if (req.method.empty() && req.id == "null") {
            std::string errLine =
                json_rpc::formatError("null", -32700, "Parse error") + "\n";
            std::cout << errLine;
            std::cout.flush(); // mandatory flush — host blocks until this arrives
            continue;
        }

        // ---- Step 3: dispatch to the handler ----
        McpResponse res = m_handler.handle(req);

        // ---- Step 4: suppress output for notifications ----
        // JSON-RPC notifications (id == "null") must NOT receive a response.
        // The handler still processes them for side effects (e.g. logging that
        // the host finished its part of the handshake).
        if (req.id == "null") continue;

        // ---- Step 5: serialize and write the response ----
        std::string responseStr = json_rpc::formatResponse(res);
        std::cerr << "[mcp_server] send: " << responseStr << "\n";

        // The trailing '\n' is part of the MCP framing — one object per line.
        std::cout << responseStr << "\n";

        // CRITICAL: flush stdout after every single response.
        // Without this the LLM host's blocking read will never return because
        // the bytes sit in the C++ stream buffer until a larger threshold is met.
        std::cout.flush();
    }

    // stdin closed — loop exits, main() performs cleanup.
    std::cerr << "[mcp_server] stdin closed, shutting down\n";
}
