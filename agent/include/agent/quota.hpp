#pragma once
#include <map>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Per-user resource limits.
struct UserQuota {
    int max_concurrent_agents{10};   // max agents running at once for this user
    int max_llm_inflight{5};         // max concurrent LLM calls
    int max_tool_inflight{20};       // max concurrent tool/action executions
};

// Thread-safe per-user quota enforcer.
// AgentManager calls this before spawning agents or dispatching LLM/tool calls.
class QuotaManager {
public:
    // Configure limits for a user.  Creates the user record if absent.
    void setQuota(const std::string& user_id, const UserQuota& quota);
    UserQuota getQuota(const std::string& user_id) const;

    // Returns true and increments the counter if under the limit.
    // Returns false (quota exceeded) without blocking.
    bool tryAcquireAgent(const std::string& user_id);
    void releaseAgent(const std::string& user_id);

    bool tryAcquireLLM(const std::string& user_id);
    void releaseLLM(const std::string& user_id);

    bool tryAcquireTool(const std::string& user_id);
    void releaseTool(const std::string& user_id);

    // Current usage snapshot for a user (for status reporting).
    nlohmann::json usageJson(const std::string& user_id) const;

private:
    struct UserState {
        UserQuota quota;
        int agents_active{0};
        int llm_inflight{0};
        int tools_inflight{0};
    };
    mutable std::mutex                   m_mutex;
    std::map<std::string, UserState>     m_users;

    UserState& getOrCreate(const std::string& user_id);
};

// RAII guard for quota slots — releases on destruction.
struct QuotaGuard {
    enum class Resource { Agent, LLM, Tool };
    QuotaGuard(QuotaManager& qm, std::string user_id, Resource res);
    ~QuotaGuard();
    QuotaGuard(const QuotaGuard&)            = delete;
    QuotaGuard& operator=(const QuotaGuard&) = delete;

private:
    QuotaManager& m_qm;
    std::string   m_user;
    Resource      m_res;
    bool          m_held{true};
};

} // namespace agent
