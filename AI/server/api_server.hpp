#pragma once

#include <string>

namespace ai_api {

// Starts the HTTP API server on the given host/port.
// Blocks until the server is stopped (SIGINT/SIGTERM).
//
// model_paths: paths to .gguf models to pre-load (may be empty; models can
//              also be added at runtime via POST /api/models).
// agents_file: path to a JSON file used to persist agents across restarts.
void RunServer(const std::string& host,
               int                port,
               const std::vector<std::string>& model_paths,
               int  context_size,
               const std::string& agents_file);

} // namespace ai_api
