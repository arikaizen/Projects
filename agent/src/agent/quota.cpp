// quota.cpp — QuotaManager and QuotaGuard implementation
#include "agent/quota.hpp"

#include <stdexcept>
#include <algorithm>

namespace agent {

// ---------------------------------------------------------------------------
// QuotaManager — getOrCreate (non-locking; caller must hold m_mutex)
// ---------------------------------------------------------------------------
QuotaManager::UserState& QuotaManager::getOrCreate(const std::string& user_id)
{
    return m_users[user_id];  // default-constructs UserState if absent
}

// ---------------------------------------------------------------------------
// setQuota
// ---------------------------------------------------------------------------
void QuotaManager::setQuota(const std::string& user_id, const UserQuota& quota)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    getOrCreate(user_id).quota = quota;
}

// ---------------------------------------------------------------------------
// getQuota
// ---------------------------------------------------------------------------
UserQuota QuotaManager::getQuota(const std::string& user_id) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_users.find(user_id);
    if (it == m_users.end()) return {};   // default limits
    return it->second.quota;
}

// ---------------------------------------------------------------------------
// Agent acquire / release
// ---------------------------------------------------------------------------
bool QuotaManager::tryAcquireAgent(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    if (s.agents_active >= s.quota.max_concurrent_agents) return false;
    ++s.agents_active;
    return true;
}

void QuotaManager::releaseAgent(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    s.agents_active = std::max(0, s.agents_active - 1);
}

// ---------------------------------------------------------------------------
// LLM acquire / release
// ---------------------------------------------------------------------------
bool QuotaManager::tryAcquireLLM(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    if (s.llm_inflight >= s.quota.max_llm_inflight) return false;
    ++s.llm_inflight;
    return true;
}

void QuotaManager::releaseLLM(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    s.llm_inflight = std::max(0, s.llm_inflight - 1);
}

// ---------------------------------------------------------------------------
// Tool acquire / release
// ---------------------------------------------------------------------------
bool QuotaManager::tryAcquireTool(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    if (s.tools_inflight >= s.quota.max_tool_inflight) return false;
    ++s.tools_inflight;
    return true;
}

void QuotaManager::releaseTool(const std::string& user_id)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto& s = getOrCreate(user_id);
    s.tools_inflight = std::max(0, s.tools_inflight - 1);
}

// ---------------------------------------------------------------------------
// usageJson
// ---------------------------------------------------------------------------
nlohmann::json QuotaManager::usageJson(const std::string& user_id) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_users.find(user_id);
    if (it == m_users.end()) {
        UserQuota dq{};
        return {
            {"user_id",               user_id},
            {"agents_active",         0},
            {"max_concurrent_agents", dq.max_concurrent_agents},
            {"llm_inflight",          0},
            {"max_llm_inflight",      dq.max_llm_inflight},
            {"tools_inflight",        0},
            {"max_tool_inflight",     dq.max_tool_inflight}
        };
    }
    const auto& s = it->second;
    return {
        {"user_id",               user_id},
        {"agents_active",         s.agents_active},
        {"max_concurrent_agents", s.quota.max_concurrent_agents},
        {"llm_inflight",          s.llm_inflight},
        {"max_llm_inflight",      s.quota.max_llm_inflight},
        {"tools_inflight",        s.tools_inflight},
        {"max_tool_inflight",     s.quota.max_tool_inflight}
    };
}

// ---------------------------------------------------------------------------
// QuotaGuard
//
// The guard is constructed AFTER a successful tryAcquire*; it is solely
// responsible for releasing the slot on destruction.
// ---------------------------------------------------------------------------
QuotaGuard::QuotaGuard(QuotaManager& qm,
                        std::string   user_id,
                        Resource      res)
    : m_qm(qm), m_user(std::move(user_id)), m_res(res), m_held(true)
{}

QuotaGuard::~QuotaGuard()
{
    if (!m_held) return;
    switch (m_res) {
        case Resource::Agent: m_qm.releaseAgent(m_user); break;
        case Resource::LLM:   m_qm.releaseLLM(m_user);   break;
        case Resource::Tool:  m_qm.releaseTool(m_user);   break;
    }
}

} // namespace agent
