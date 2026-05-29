#include "grep_action.hpp"
#include "agent/agent_context.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace agent {

// ── ripgrep availability ─────────────────────────────────────────────────────

bool GrepAction::ripgrepAvailable() {
    FILE* p = popen("rg --version 2>/dev/null", "r");
    if (!p) return false;
    std::array<char, 64> buf{};
    bool found = (fgets(buf.data(), static_cast<int>(buf.size()), p) != nullptr);
    pclose(p);
    return found;
}

// ── ripgrep backend ──────────────────────────────────────────────────────────

nlohmann::json GrepAction::runRipgrep(const std::string& pattern,
                                       const std::string& path,
                                       bool               use_regex) {
    // Build command: rg --line-number [--fixed-strings] PATTERN PATH
    std::string cmd = "rg --line-number --with-filename --no-heading";
    if (!use_regex) cmd += " --fixed-strings";
    // Quote pattern and path with single quotes; escape any embedded single quotes.
    auto shellEscape = [](const std::string& s) {
        std::string out = "'";
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else           out += c;
        }
        out += "'";
        return out;
    };
    cmd += " " + shellEscape(pattern);
    cmd += " " + shellEscape(path);
    cmd += " 2>/dev/null";

    nlohmann::json matches = nlohmann::json::array();

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return matches;

    std::array<char, 4096> buf{};
    std::string line;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        line = buf.data();
        // Remove trailing newline.
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        // rg format: file:line_number:content
        auto colon1 = line.find(':');
        if (colon1 == std::string::npos) continue;
        auto colon2 = line.find(':', colon1 + 1);
        if (colon2 == std::string::npos) continue;

        std::string file_part    = line.substr(0, colon1);
        std::string ln_str       = line.substr(colon1 + 1, colon2 - colon1 - 1);
        std::string content_part = line.substr(colon2 + 1);
        int ln = 0;
        try { ln = std::stoi(ln_str); } catch (...) {}
        matches.push_back({{"file", file_part}, {"line_number", ln}, {"line", content_part}});
    }
    pclose(pipe);
    return matches;
}

// ── manual fallback ──────────────────────────────────────────────────────────

static void grepFile(const std::string&     file_path,
                     const std::string&     pattern,
                     bool                   use_regex,
                     const std::regex&      re,
                     nlohmann::json&        matches) {
    std::ifstream file(file_path);
    if (!file.is_open()) return;

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        bool hit = false;
        if (use_regex) {
            hit = std::regex_search(line, re);
        } else {
            hit = (line.find(pattern) != std::string::npos);
        }
        if (hit) {
            matches.push_back({
                {"file", file_path},
                {"line_number", line_number},
                {"line", line}
            });
        }
    }
}

nlohmann::json GrepAction::manualGrep(const std::string& pattern,
                                       const std::string& path,
                                       bool               use_regex) {
    nlohmann::json matches = nlohmann::json::array();

    std::regex re;
    if (use_regex) {
        try {
            re = std::regex(pattern, std::regex::ECMAScript | std::regex::optimize);
        } catch (const std::regex_error& e) {
            // Invalid regex — fall back to literal search.
            use_regex = false;
        }
    }

    std::filesystem::path fs_path(path);
    std::error_code ec;

    if (!std::filesystem::exists(fs_path, ec) || ec) {
        return matches;
    }

    if (std::filesystem::is_regular_file(fs_path, ec) && !ec) {
        grepFile(path, pattern, use_regex, re, matches);
    } else if (std::filesystem::is_directory(fs_path, ec) && !ec) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 fs_path,
                 std::filesystem::directory_options::skip_permission_denied,
                 ec)) {
            if (ec) { ec.clear(); continue; }
            if (entry.is_regular_file(ec) && !ec) {
                grepFile(entry.path().string(), pattern, use_regex, re, matches);
            }
        }
    }

    return matches;
}

// ── execute ──────────────────────────────────────────────────────────────────

WorkResult GrepAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved  = ctx.resolveReferences(inputs);
        auto pattern   = resolved.at("pattern").get<std::string>();
        auto path      = resolved.value("path", std::string("."));
        bool use_regex = resolved.value("regex", false);

        std::cerr << "[ACTION:" << name << "] grep pattern=\"" << pattern
                  << "\" path=\"" << path << "\" regex=" << use_regex << "\n";

        nlohmann::json matches;
        if (ripgrepAvailable()) {
            matches = runRipgrep(pattern, path, use_regex);
        } else {
            matches = manualGrep(pattern, path, use_regex);
        }

        result.success = true;
        result.output  = {{"matches", matches}, {"count", static_cast<int>(matches.size())}};
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerGrepAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "GrepAction",
        "Search file contents for a pattern. Uses ripgrep if available, otherwise falls back to built-in search.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"pattern"}},
            {"properties", {
                {"pattern", {{"type", "string"}, {"description", "Search pattern."}}},
                {"path",    {{"type", "string"}, {"description", "File or directory to search (default \".\")."}}},
                {"regex",   {{"type", "boolean"}, {"description", "Treat pattern as a regular expression (default false)."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<GrepAction>(std::move(id), "GrepAction", std::move(inputs));
    });
}

} // namespace agent
