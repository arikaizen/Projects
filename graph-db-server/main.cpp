#include "persistence.h"
#include "graph_store.h"
#include "request_handler.h"
#include "server.h"

#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>

int main(int argc, char* argv[]) {
    std::string filePath = "graph.jsonl";
    int port = 7474;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            filePath = argv[++i];
        }
    }

    try {
        Persistence persistence(filePath);
        Graph graph = persistence.load();
        GraphStore store(graph, persistence);
        RequestHandler handler(store);
        Server server(port, handler);

        std::cout << "Graph DB server running on port " << port << "\n";
        std::cout << "Persistence file: " << filePath << "\n";
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
