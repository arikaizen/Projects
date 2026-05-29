// work_item.cpp — WorkResult serialisation, dependency scanning, WorkItem helpers
#include "agent/work_item.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace agent {

// ---------------------------------------------------------------------------
// Internal helper: format a time_point as an ISO-8601 UTC string.
// ---------------------------------------------------------------------------
static std::string toISO8601(std::chrono::system_clock::time_point tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm     tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ---------------------------------------------------------------------------
// WorkResult::toJson
// ---------------------------------------------------------------------------
nlohmann::json WorkResult::toJson() const
{
    return {
        {"item_id",         item_id},
        {"item_name",       item_name},
        {"item_kind",       item_kind},
        {"success",         success},
        {"output",          output},
        {"error",           error},
        {"timestamp",       toISO8601(timestamp)},
        {"duration_ms",     static_cast<long long>(duration.count())},
        {"iteration",       iteration},
        {"ran_in_parallel", ran_in_parallel},
        {"skipped_reason",  skipped_reason}
    };
}

// ---------------------------------------------------------------------------
// WorkItem::scanForRefs
//
// Recursively walk `j`.  Whenever a string value matches  ^\$([A-Za-z0-9_]+)
// (optionally followed by a dot-path) the bare id (the capture group) is
// inserted into `out`.
// ---------------------------------------------------------------------------
void WorkItem::scanForRefs(const nlohmann::json& j, std::set<std::string>& out)
{
    if (j.is_string()) {
        // Pattern: starts with $ then an identifier, optionally .field
        static const std::regex kRefRe(R"(^\$([A-Za-z0-9_]+))");
        const std::string& s = j.get<std::string>();
        std::smatch m;
        if (std::regex_search(s, m, kRefRe)) {
            out.insert(m[1].str());
        }
        return;
    }

    if (j.is_array()) {
        for (const auto& elem : j) {
            scanForRefs(elem, out);
        }
        return;
    }

    if (j.is_object()) {
        for (const auto& [key, val] : j.items()) {
            scanForRefs(val, out);
        }
        return;
    }

    // primitive (number, bool, null) — nothing to scan
}

// ---------------------------------------------------------------------------
// WorkItem::dependencies
// ---------------------------------------------------------------------------
std::set<std::string> WorkItem::dependencies() const
{
    std::set<std::string> refs;
    scanForRefs(inputs, refs);
    return refs;
}

// ---------------------------------------------------------------------------
// WorkItem::toSummaryJson
// ---------------------------------------------------------------------------
nlohmann::json WorkItem::toSummaryJson() const
{
    return {
        {"id",          id},
        {"name",        name},
        {"kind",        kind() == Kind::Stage ? "Stage" : "Action"},
        {"description", description()}
    };
}

} // namespace agent
