#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

// Executes a Python tool script as a child process.
//
// Protocol:
//   stdin  → one-line JSON: {"args": {...}, "api_keys": {...}}
//   stdout ← one-line JSON: {"success": true,  "result": ...}
//                        or {"success": false, "error": "message"}
//
// The C++ server supplies api_keys from its own environment so tool scripts
// never need to hard-code credentials.
class ToolRunner {
public:
    struct Result {
        bool           success{false};
        nlohmann::json result;
        std::string    error;
    };

    // tools_dir: directory containing the Python scripts
    explicit ToolRunner(std::filesystem::path tools_dir);

    Result run(const std::string& script_name,
               const nlohmann::json& args,
               int timeout_seconds = 30) const;

private:
    std::filesystem::path m_tools_dir;

    // Collects API key environment variables to pass to the subprocess.
    static nlohmann::json collect_api_keys();
};
