#pragma once
#include "agent.hpp"
#include "agent_context.hpp"
#include "blackboard.hpp"
#include "event_bus.hpp"
#include "llm_client.hpp"
#include "memory_backend.hpp"
#include "message_inbox.hpp"
#include "prompt_loader.hpp"
#include "quota.hpp"
#include "thread_pool.hpp"
#include "work_factory.hpp"
#include <filesystem>
#include <future>
#include <map>
#include <thread>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

struct MCPServerConfig {
    std::string    name;
    std::string    url;           // Base URL for HTTP transport, e.g. "http://localhost:8081"
    std::string    bearer_token;  // Access token attached to every JSON-RPC request (step ③)
    std::string    transport{"http"}; // "http" | "stdio"
    nlohmann::json extra;
};

// Central orchestrator.  Owns all shared infrastructure; spawns and manages
// agents; enforces per-user quotas; exposes Patterns A/B/C.
//
// Thread-safety contract:
//   All public methods are safe to call concurrently from any thread.
//   Internal agent registries, pipe tables, MCP tables, and quota tables each
//   have their own mutex.  The shared pool, blackboard, event bus, and prompt
//   loader are individually thread-safe (see their respective headers).
class AgentManager {
public:
    struct Config {
        std::filesystem::path prompts_dir{"./prompts"};
        int                   thread_pool_size{16};
        int                   max_agent_depth{3};
        std::string           default_user_id{"default"};
    };

    explicit AgentManager(Config                         config,
                          std::shared_ptr<LLMClient>     llm,
                          std::shared_ptr<MemoryBackend> memory = nullptr);
    ~AgentManager();

    AgentManager(const AgentManager&)            = delete;
    AgentManager& operator=(const AgentManager&) = delete;

    // ── Infrastructure accessors ─────────────────────────────────────────────
    WorkFactory&   factory()      { return *m_factory; }
    PromptLoader&  promptLoader() { return *m_prompt_loader; }
    EventBus&      eventBus()     { return *m_event_bus; }
    Blackboard&    blackboard()   { return *m_blackboard; }
    ThreadPool&    threadPool()   { return *m_pool; }
    QuotaManager&  quotaManager() { return *m_quota; }

    // ── Agent lifecycle ──────────────────────────────────────────────────────
    // config_json carries user_id; quotas are enforced per user.
    std::string spawnAgent(const AgentConfig& cfg);
    void        destroyAgent(const std::string& agent_id);

    // Runs the agent on the thread pool.  Returns a future for the final output.
    std::future<nlohmann::json> runAgent(const std::string& agent_id,
                                         const std::string& task);
    nlohmann::json runAgentBlocking(const std::string& agent_id,
                                    const std::string& task);

    nlohmann::json listAgents(const std::string& user_id = "") const;
    nlohmann::json getStatus(const std::string& agent_id) const;
    void           cancelAgent(const std::string& agent_id);

    // ── Pattern A — delegation / piping ─────────────────────────────────────
    void pipe(const std::string& from_id,
              const std::string& to_id,
              const std::string& template_string);

    // ── Pattern B — messaging ────────────────────────────────────────────────
    void                  sendMessage(const std::string& from_id,
                                      const std::string& to_id,
                                      const nlohmann::json& msg);
    void                  broadcast(const std::string& from_id,
                                    const nlohmann::json& msg);
    std::vector<Message>  drainInbox(const std::string& agent_id);

    // ── Pattern C — blackboard (delegates to m_blackboard) ──────────────────
    void                     blackboardWrite(const std::string& key, nlohmann::json v);
    nlohmann::json           blackboardRead(const std::string& key);
    std::vector<std::string> blackboardKeys(const std::string& prefix = "");
    void                     blackboardDelete(const std::string& key);

    // ── Composition — fan-out / fan-in ───────────────────────────────────────
    std::vector<std::future<nlohmann::json>> fanOut(
        const std::vector<AgentConfig>& configs,
        const std::string&              shared_task);

    nlohmann::json fanIn(
        std::vector<std::future<nlohmann::json>>& futures,
        const AgentConfig&                        synthesizer_config);

    nlohmann::json researchFromAngles(
        const std::vector<std::string>& angles,
        const std::string&              topic);

    // ── Real-time injection ──────────────────────────────────────────────────
    void injectWork(const std::string& agent_id, std::unique_ptr<WorkItem> item);

    // ── Event subscription ───────────────────────────────────────────────────
    void subscribeEvents(EventCallback cb, void* key = nullptr);
    void unsubscribeEvents(void* key);

    // ── Prompt hot reload ────────────────────────────────────────────────────
    void reloadPrompts();
    void setPromptsDir(const std::filesystem::path& dir);

    // ── Multi-tenancy ────────────────────────────────────────────────────────
    void setUserQuota(const std::string& user_id, const UserQuota& quota);

    // ── MCP management ───────────────────────────────────────────────────────
    void           connectMCP(const MCPServerConfig& cfg);
    void           disconnectMCP(const std::string& server_name);
    nlohmann::json listMCPServers() const;

private:
    Config m_config;

    std::shared_ptr<LLMClient>     m_llm;
    std::shared_ptr<MemoryBackend> m_memory;
    std::shared_ptr<WorkFactory>   m_factory;
    std::shared_ptr<PromptLoader>  m_prompt_loader;
    std::shared_ptr<EventBus>      m_event_bus;
    std::shared_ptr<Blackboard>    m_blackboard;
    std::shared_ptr<ThreadPool>    m_pool;
    std::shared_ptr<QuotaManager>  m_quota;

    // ── Agent registry ───────────────────────────────────────────────────────
    struct AgentEntry {
        AgentConfig                  config;
        std::unique_ptr<Agent>       agent;
        std::unique_ptr<MessageInbox> inbox;
        std::string                  status; // "idle"|"running"|"done"|"failed"
        nlohmann::json               result;
        std::string                  user_id;
        // Each agent loop runs on its own dedicated thread (Level 1/2
        // concurrency).  The shared ThreadPool is reserved for intra-batch
        // (Level 3) parallelism, so agent loops never contend with — or
        // deadlock against — the work they themselves submit to the pool.
        std::thread                  runner;
    };
    mutable std::mutex                      m_agents_mutex;
    std::map<std::string, AgentEntry>       m_agents;
    int                                     m_agent_counter{0};

    // ── Pipe registry ────────────────────────────────────────────────────────
    struct PipeEntry { std::string from_id, to_id, template_str; };
    mutable std::mutex        m_pipes_mutex;
    std::vector<PipeEntry>    m_pipes;

    // ── MCP registry ────────────────────────────────────────────────────────
    mutable std::mutex                        m_mcp_mutex;
    std::map<std::string, MCPServerConfig>    m_mcp_servers;

    // ── Helpers ──────────────────────────────────────────────────────────────
    void registerBuiltinItems();
    void onAgentFinished(const std::string& agent_id, const nlohmann::json& result);

    AgentEntry&       getEntry(const std::string& agent_id);       // throws if absent
    const AgentEntry& getEntryConst(const std::string& id) const;

    std::unique_ptr<AgentContext> makeContext(const AgentEntry& entry);
};

} // namespace agent
