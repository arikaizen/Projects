#include "agent/plan_cache.hpp"
#include <fstream>
#include <stdexcept>

namespace agent {

PlanCache::PlanCache(std::filesystem::path cache_dir)
    : m_cache_dir(std::move(cache_dir))
{
    std::filesystem::create_directories(m_cache_dir);
}

std::filesystem::path PlanCache::cachePath(const std::string& agent_id) const
{
    std::string safe = agent_id;
    for (char& c : safe)
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') c = '_';
    return m_cache_dir / (safe + ".json");
}

void PlanCache::save(const std::string& agent_id, const CachedPlan& plan)
{
    nlohmann::json j;
    j["task"]        = plan.task;
    j["fingerprint"] = plan.fingerprint;
    j["steps"]       = plan.steps;
    j["created_at"]  = plan.created_at;
    j["run_count"]   = plan.run_count;

    std::unique_lock<std::mutex> lock(m_mutex);
    std::ofstream f(cachePath(agent_id));
    if (!f.is_open())
        throw std::runtime_error("PlanCache: cannot write cache for agent '" + agent_id + "'");
    f << j.dump(2);
}

std::optional<CachedPlan> PlanCache::load(const std::string& agent_id) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto path = cachePath(agent_id);
    if (!std::filesystem::exists(path)) return std::nullopt;

    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;

    try {
        nlohmann::json j = nlohmann::json::parse(f);
        CachedPlan plan;
        plan.task        = j.value("task",        std::string{});
        plan.fingerprint = j.value("fingerprint", nlohmann::json::object());
        plan.steps       = j.value("steps",       nlohmann::json::array());
        plan.created_at  = j.value("created_at",  std::string{});
        plan.run_count   = j.value("run_count",   0);
        return plan;
    } catch (...) {
        return std::nullopt;
    }
}

void PlanCache::clear(const std::string& agent_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto path = cachePath(agent_id);
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
}

bool PlanCache::exists(const std::string& agent_id) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return std::filesystem::exists(cachePath(agent_id));
}

} // namespace agent
