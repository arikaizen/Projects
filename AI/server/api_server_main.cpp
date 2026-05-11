// api_server_main.cpp
// Entry point for the AI REST API server.
//
// Usage:
//   ./bin/ai_server [options] [model1.gguf ...]
//   ./bin/ai_server --host 0.0.0.0 --port 8080 model.gguf
//
// Options:
//   --host <addr>      Bind address (default: 127.0.0.1)
//   --port <n>         Port (default: 8080)
//   --ctx  <n>         Context size (default: 4096)
//   --agents <path>    Agents persistence file (default: agents.json)

#include "api_server.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string host       = "127.0.0.1";
    int         port       = 8080;
    int         ctx        = 4096;
    std::string agents_file= "agents.json";
    std::vector<std::string> models;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--host"   && i + 1 < argc) { host        = argv[++i]; }
        else if (a == "--port"   && i + 1 < argc) { port        = std::stoi(argv[++i]); }
        else if (a == "--ctx"    && i + 1 < argc) { ctx         = std::stoi(argv[++i]); }
        else if (a == "--agents" && i + 1 < argc) { agents_file = argv[++i]; }
        else if (a[0] != '-') { models.push_back(a); }
        else {
            std::cerr << "Unknown option: " << a << "\n";
            std::cerr << "Usage: " << argv[0]
                      << " [--host H] [--port P] [--ctx N] [--agents FILE] [model.gguf ...]\n";
            return 1;
        }
    }

    ai_api::RunServer(host, port, models, ctx, agents_file);
    return 0;
}
