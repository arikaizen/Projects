#include "tool_runner.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unistd.h>

using json = nlohmann::json;

// List of API-key env vars forwarded to tool subprocesses.
static const char* API_KEY_VARS[] = {
    "GOOGLE_API_KEY",
    "GOOGLE_CSE_ID",
    "OPENWEATHER_API_KEY",
    "NEWSAPI_KEY",
    "ALPHA_VANTAGE_KEY",
    "GITHUB_TOKEN",
    "DEEPL_API_KEY",
    "WOLFRAM_APP_ID",
    "YOUTUBE_API_KEY",
    "BING_SEARCH_KEY",
    "UNSPLASH_ACCESS_KEY",
    "IPINFO_TOKEN",
    nullptr,
};

ToolRunner::ToolRunner(std::filesystem::path tools_dir)
    : m_tools_dir(std::move(tools_dir)) {}

json ToolRunner::collect_api_keys() {
    json keys = json::object();
    for (const char** k = API_KEY_VARS; *k; ++k) {
        const char* v = std::getenv(*k);
        if (v) keys[*k] = v;
    }
    return keys;
}

ToolRunner::Result ToolRunner::run(const std::string& script_name,
                                    const json& args,
                                    int /*timeout_seconds*/) const {
    auto script_path = m_tools_dir / script_name;
    if (!std::filesystem::exists(script_path)) {
        return {false, {}, "Tool script not found: " + script_path.string()};
    }

    json input_doc = {
        {"args",     args},
        {"api_keys", collect_api_keys()},
    };
    std::string input_line = input_doc.dump() + "\n";

    // Build command: python3 <script_path>
    std::string cmd = "python3 " + script_path.string();

    // Open a bidirectional pipe via popen (write-only here; we use a temp
    // approach: write to stdin via a process open for writing, capture stdout
    // by redirecting stderr to stdout in the shell command).
    // Full bidirectional I/O uses popen with a wrapper shell command:
    //   echo '<JSON>' | python3 <script>
    // We shell-escape the JSON by writing it to a temp file.

    // Use mkstemp for the input file to avoid shell injection
    char tmpfile[] = "/tmp/mcp_tool_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) return {false, {}, "mkstemp failed"};
    {
        FILE* f = fdopen(fd, "w");
        if (!f) { close(fd); return {false, {}, "fdopen failed"}; }
        fwrite(input_line.data(), 1, input_line.size(), f);
        fclose(f);
    }

    std::string full_cmd = "python3 " + script_path.string() + " < " + tmpfile + " 2>/dev/null";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        std::remove(tmpfile);
        return {false, {}, "popen failed"};
    }

    std::string output;
    std::array<char, 4096> buf;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        output += buf.data();
    pclose(pipe);
    std::remove(tmpfile);

    if (output.empty()) return {false, {}, "Tool produced no output"};

    try {
        auto resp = json::parse(output);
        bool ok   = resp.value("success", false);
        return {ok,
                ok ? resp.value("result", json{}) : json{},
                ok ? "" : resp.value("error", "unknown error")};
    } catch (const std::exception& e) {
        return {false, {}, std::string("JSON parse error: ") + e.what() +
                           " — raw: " + output.substr(0, 200)};
    }
}
