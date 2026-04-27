/**
 * @file main.cpp
 * @brief Entry point — wires all MCP server components together and starts the loop.
 *
 * Startup sequence
 * ----------------
 *   1. Parse optional --host and --port command-line arguments.
 *   2. Construct a DbClient and attempt to connect to the graph DB server.
 *      Exit with code 1 and a clear stderr message if the connection fails.
 *   3. Construct ToolRegistry, McpHandler, and McpServer.
 *   4. Log "MCP server ready" to stderr (NEVER to stdout).
 *   5. Enter the blocking McpServer::run() loop.
 *   6. On exit (stdin EOF), disconnect from the graph DB cleanly.
 *
 * stdout purity
 * -------------
 * Every message on stdout must be a valid MCP JSON-RPC line.  A single stray
 * character (even a space or newline) before the first JSON object will corrupt
 * the handshake.  ALL logging, errors, and debug output go to stderr only.
 *
 * Command-line arguments
 * ----------------------
 *   --host <addr>   IPv4 address or hostname of the graph DB (default: 127.0.0.1)
 *   --port <n>      TCP port of the graph DB server (default: 7474)
 *
 * Claude Desktop configuration
 * ----------------------------
 * Add to claude_desktop_config.json:
 * @code
 *   {
 *     "mcpServers": {
 *       "my-graph-db": {
 *         "command": "/path/to/mcp_server",
 *         "args": ["--host", "127.0.0.1", "--port", "7474"]
 *       }
 *     }
 *   }
 * @endcode
 */

#include "db_client.h"
#include "tool_registry.h"
#include "mcp_handler.h"
#include "mcp_server.h"

#include <iostream>    // std::cerr (stdout is reserved for MCP protocol)
#include <string>
#include <cstring>     // std::strcmp

int main(int argc, char* argv[]) {

    // -----------------------------------------------------------------------
    // Argument parsing
    // -----------------------------------------------------------------------
    std::string dbHost = "127.0.0.1";
    int dbPort = 7474;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            dbHost = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            dbPort = std::stoi(argv[++i]); // throws on non-numeric, caught below
        }
    }

    // -----------------------------------------------------------------------
    // Connect to the graph DB server
    //
    // If the graph DB is not reachable we cannot serve any tools — exit
    // immediately with a clear message rather than starting up and silently
    // failing on every tool call.
    // -----------------------------------------------------------------------
    DbClient db(dbHost, dbPort);
    if (!db.connect()) {
        std::cerr << "Fatal: failed to connect to graph DB at "
                  << dbHost << ":" << dbPort << "\n"
                  << "  Make sure the graph DB server is running before\n"
                  << "  starting the MCP server.\n";
        return 1;
    }
    std::cerr << "[main] connected to graph DB at "
              << dbHost << ":" << dbPort << "\n";

    // -----------------------------------------------------------------------
    // Construct the processing pipeline
    //
    // Lifetime order (each object depends on the one above it staying alive):
    //   db          — must outlive everything (socket to the DB)
    //   registry    — pure data, no dependencies
    //   handler     — depends on db and registry
    //   server      — depends on handler
    // -----------------------------------------------------------------------
    ToolRegistry registry;
    McpHandler   handler(registry, db);
    McpServer    server(handler);

    // Log to stderr only — stdout must stay silent until the first JSON-RPC
    // message so the MCP host's handshake parser sees a clean stream.
    std::cerr << "[main] MCP server ready, waiting for requests on stdin\n";

    // -----------------------------------------------------------------------
    // Run — blocks until stdin is closed by the MCP host
    // -----------------------------------------------------------------------
    server.run();

    // -----------------------------------------------------------------------
    // Clean shutdown — close TCP connection to the graph DB
    // -----------------------------------------------------------------------
    db.disconnect();
    std::cerr << "[main] shutdown complete\n";

    return 0;
}
