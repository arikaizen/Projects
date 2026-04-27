/**
 * @file main.cpp
 * @brief Entry point — wires all components together and starts the server.
 *
 * Startup sequence
 * ----------------
 *   1. Parse optional command-line arguments (--port, --file).
 *   2. Construct a Persistence instance bound to the JSONL file path.
 *   3. Load the graph from disk (returns an empty Graph if the file is absent).
 *   4. Construct a GraphStore, transferring ownership of the loaded Graph.
 *   5. Construct a RequestHandler bound to the GraphStore.
 *   6. Construct a Server bound to the RequestHandler.
 *   7. Start the blocking event loop via Server::run().
 *
 * All objects are constructed on the stack in dependency order so that C++
 * RAII guarantees correct destruction in reverse order if an exception occurs
 * before Server::run() is called.
 *
 * Command-line options
 * --------------------
 *   --port <n>    TCP port to listen on (default: 7474)
 *   --file <path> Path to the JSONL persistence file (default: graph.jsonl)
 *
 * Example usage
 * -------------
 *   ./graph_db_server
 *   ./graph_db_server --port 9000 --file /var/db/knowledge.jsonl
 */

#include "persistence.h"
#include "graph_store.h"
#include "request_handler.h"
#include "server.h"

#include <iostream>    // std::cout, std::cerr
#include <string>
#include <cstring>     // std::strcmp
#include <stdexcept>   // std::exception

int main(int argc, char* argv[]) {

    // -----------------------------------------------------------------------
    // Defaults
    // -----------------------------------------------------------------------
    std::string filePath = "graph.jsonl"; // JSONL persistence file
    int port = 7474;                      // default port (mirrors MCP memory server)

    // -----------------------------------------------------------------------
    // Command-line argument parsing
    //
    // Simple linear scan; unknown flags are silently ignored.  Each recognized
    // flag consumes the following token as its value (i + 1 < argc guard
    // prevents reading past the end of argv).
    // -----------------------------------------------------------------------
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::stoi(argv[++i]); // std::stoi throws on non-numeric input
        } else if (std::strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            filePath = argv[++i];
        }
    }

    // -----------------------------------------------------------------------
    // Component construction and server startup
    //
    // Wrapped in a single try/catch so any fatal error (port already in use,
    // file permission denied, etc.) is reported cleanly before exit.
    // -----------------------------------------------------------------------
    try {
        // Bind the persistence layer to the chosen file path.
        // No I/O happens here; the file is opened lazily in save() / load().
        Persistence persistence(filePath);

        // Restore the graph state from disk.  Returns an empty Graph on first run
        // (when graph.jsonl does not yet exist).
        Graph graph = persistence.load();

        // GraphStore takes ownership of the graph (via move) and holds a
        // reference to persistence for write-through saves on every mutation.
        GraphStore store(graph, persistence);

        // The request handler maps protocol commands to GraphStore operations.
        RequestHandler handler(store);

        // The server owns the TCP socket lifecycle.  It holds a reference to
        // handler; the actual socket is not opened until run() is called.
        Server server(port, handler);

        std::cout << "Graph DB server running on port " << port << "\n";
        std::cout << "Persistence file: " << filePath << "\n";

        // Blocking call — does not return under normal operation.
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1; // non-zero exit signals failure to the shell / process supervisor
    }

    return 0;
}
