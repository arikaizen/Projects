#pragma once
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Snapshot of a successful agent run: fingerprint (structured understanding)
// used to detect parameter changes, plus the raw plan steps as the LLM produced
// them (pre-$ref-resolution) so they can be replayed or adapted.
struct CachedPlan {
    std::string    task;          // original task string
    nlohmann::json fingerprint;  // agent:understanding JSON from the successful run
    nlohmann::json steps;        // raw plan array returned by ReasonStage/PlanAdaptStage
    std::string    created_at;   // ISO-8601 UTC
    int            run_count{0}; // how many times this plan has been replayed
};

// File-backed per-agent plan storage.  One JSON file per agent_id written to
// cache_dir.  Thread-safe — callers do not need external locking.
class PlanCache {
public:
    explicit PlanCache(std::filesystem::path cache_dir);

    void save(const std::string& agent_id, const CachedPlan& plan);
    std::optional<CachedPlan> load(const std::string& agent_id) const;
    void clear(const std::string& agent_id);
    bool exists(const std::string& agent_id) const;

private:
    std::filesystem::path cachePath(const std::string& agent_id) const;

    std::filesystem::path m_cache_dir;
    mutable std::mutex    m_mutex;
};

} // namespace agent
