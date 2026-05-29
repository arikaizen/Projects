#include "glob_action.hpp"
#include "agent/agent_context.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace agent {

// ── Wildcard matching ────────────────────────────────────────────────────────
// Handles * (match any substring) and ? (match any single char).
// Matching is done against the FULL relative path string so that patterns like
// "src/*.cpp" can cross-match against relative paths such as "src/foo.cpp".
bool GlobAction::wildcardMatch(const std::string& pattern, const std::string& name) {
    // Use classic DP / recursive approach with memoization via index pairs.
    // For the sizes encountered in practice (patterns < 200 chars), this is fine.
    const size_t plen = pattern.size();
    const size_t nlen = name.size();

    // dp[i][j] = pattern[0..i) matches name[0..j)
    std::vector<std::vector<bool>> dp(plen + 1, std::vector<bool>(nlen + 1, false));
    dp[0][0] = true;

    // A leading sequence of '*' can match the empty string.
    for (size_t i = 1; i <= plen; ++i) {
        if (pattern[i - 1] == '*') {
            dp[i][0] = dp[i - 1][0];
        }
    }

    for (size_t i = 1; i <= plen; ++i) {
        for (size_t j = 1; j <= nlen; ++j) {
            char p = pattern[i - 1];
            char n = name[j - 1];
            if (p == '*') {
                dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
            } else if (p == '?' || p == n) {
                dp[i][j] = dp[i - 1][j - 1];
            }
        }
    }

    return dp[plen][nlen];
}

// ── execute ──────────────────────────────────────────────────────────────────

WorkResult GlobAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto pattern  = resolved.at("pattern").get<std::string>();
        auto root     = resolved.value("root", std::string("."));

        std::cerr << "[ACTION:" << name << "] glob pattern=\"" << pattern
                  << "\" root=\"" << root << "\"\n";

        nlohmann::json matches = nlohmann::json::array();

        std::filesystem::path root_path(root);
        if (!std::filesystem::exists(root_path)) {
            result.success = true;
            result.output  = {{"matches", matches}, {"count", 0}};
            result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
            result.timestamp = system_clock::now();
            return result;
        }

        std::error_code ec;
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 root_path, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) { ec.clear(); continue; }

            // Match against the filename only OR the full relative path.
            // First try to match the filename (basename), then the full
            // relative path — whichever succeeds counts as a hit.
            auto rel = std::filesystem::relative(entry.path(), root_path, ec);
            if (ec) { ec.clear(); continue; }

            std::string rel_str  = rel.string();
            std::string base_str = entry.path().filename().string();

            if (wildcardMatch(pattern, base_str) || wildcardMatch(pattern, rel_str)) {
                matches.push_back(entry.path().string());
            }
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

void registerGlobAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "GlobAction",
        "Recursively find files matching a wildcard pattern (* and ? supported).",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"pattern"}},
            {"properties", {
                {"pattern", {{"type", "string"}, {"description", "Glob pattern, e.g. \"*.cpp\" or \"src/*.hpp\"."}}},
                {"root",    {{"type", "string"}, {"description", "Root directory to search (default \".\")."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<GlobAction>(std::move(id), "GlobAction", std::move(inputs));
    });
}

} // namespace agent
