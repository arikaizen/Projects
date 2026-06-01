#pragma once
#include "work_item.hpp"
#include "llm_client.hpp"
#include "work_factory.hpp"
#include "prompt_loader.hpp"
#include "blackboard.hpp"
#include "message_inbox.hpp"
#include "event_bus.hpp"
#include "memory_backend.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

class AgentManager;  // forward declaration
class PlanCache;     // forward declaration

struct AgentConfig {
    std::string    agent_id;
    std::string    name;
    std::string    task;
    int            max_iterations{100};
    int            max_depth{3};
    int            current_depth{0};
    nlohmann::json extra;
};

// Per-agent runtime state.  Owns the queue, history, and all context an agent
// needs without reaching back into AgentManager on the hot path.
class AgentContext {
public:
    explicit AgentContext(
        AgentConfig                    config,
        std::shared_ptr<LLMClient>     llm,
        std::shared_ptr<WorkFactory>   factory,
        std::shared_ptr<PromptLoader>  prompt_loader,
        std::shared_ptr<MemoryBackend> memory,
        Blackboard*                    blackboard,
        MessageInbox*                  inbox,
        EventBus*                      event_bus,
        AgentManager*                  manager,
        PlanCache*                     plan_cache = nullptr);

    // ── Queue ────────────────────────────────────────────────────────────────

    enum class Position { Front, Back };

    // Push from the agent's own execution thread.
    void push(std::unique_ptr<WorkItem> item, Position pos = Position::Back);

    // Thread-safe entry point for pushes from outside the agent thread.
    void injectFromOutside(std::unique_ptr<WorkItem> item,
                           Position pos = Position::Front);

    // Pop next item.  Blocks until one is available or termination is signalled.
    // Returns nullptr when the agent should stop.
    std::unique_ptr<WorkItem> pop();

    // Non-blocking pop — returns nullptr immediately if the queue is empty.
    // Used by the Agent loop to drain items pushed by a Stage in one round,
    // so they can be executed together as a dependency-aware parallel batch.
    std::unique_ptr<WorkItem> try_pop();

    bool   queueEmpty()  const;
    size_t queueSize()   const;
    nlohmann::json queueSummaryJson() const;

    // Wake the pop() waiter (call when cancelling or stopping externally).
    void wakeLoop();

    // ── History ──────────────────────────────────────────────────────────────

    void recordResult(WorkResult result);

    const WorkResult* lastResult()              const;
    const WorkResult* resultById(const std::string& id) const;

    // Human-readable JSON summary of recent history for the LLM prompt.
    nlohmann::json historySummaryJson(int max_entries = 20) const;

    const std::vector<WorkResult>& history() const;

    // ── Reference resolution ─────────────────────────────────────────────────

    // Walk `inputs` and replace strings matching ^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$
    // with values from history.  Throws std::runtime_error if a ref is unresolvable.
    nlohmann::json resolveReferences(const nlohmann::json& inputs) const;

    // ── ID uniqueness ────────────────────────────────────────────────────────

    bool idExists(const std::string& id) const;

    // ── Termination flags ────────────────────────────────────────────────────

    std::atomic<bool> cancellation_flag{false};
    bool              should_stop{false};
    nlohmann::json    final_output;
    int               iteration_count{0};

    // ── Accessors ────────────────────────────────────────────────────────────

    const AgentConfig& config()        const { return m_config; }
    LLMClient&         llm()                 { return *m_llm; }
    WorkFactory&       factory()             { return *m_factory; }
    PromptLoader&      promptLoader()        { return *m_prompt_loader; }
    MemoryBackend&     memory()              { return *m_memory; }
    Blackboard*        blackboard()          { return m_blackboard; }
    MessageInbox*      inbox()               { return m_inbox; }
    EventBus*          eventBus()            { return m_event_bus; }
    AgentManager*      manager()             { return m_manager; }
    PlanCache*         planCache()           { return m_plan_cache; }

    // ── Per-agent todo list ──────────────────────────────────────────────────

    std::vector<std::string> todo_list;

private:
    AgentConfig                    m_config;
    std::shared_ptr<LLMClient>     m_llm;
    std::shared_ptr<WorkFactory>   m_factory;
    std::shared_ptr<PromptLoader>  m_prompt_loader;
    std::shared_ptr<MemoryBackend> m_memory;
    Blackboard*                    m_blackboard;
    MessageInbox*                  m_inbox;
    EventBus*                      m_event_bus;
    AgentManager*                  m_manager;
    PlanCache*                     m_plan_cache;

    mutable std::mutex              m_queue_mutex;
    std::condition_variable         m_queue_cv;
    std::deque<std::unique_ptr<WorkItem>> m_queue;

    // How long an idle agent waits for new work before terminating with
    // QueueEmpty.  Items injected while the agent is busy are caught by the
    // try_pop() drain, so this only bounds idle-termination latency.
    int                             m_idle_grace_ms{150};

    mutable std::mutex              m_history_mutex;
    std::vector<WorkResult>         m_history;

    // Helper: resolve a single "$ref" or "$ref.field" string.
    nlohmann::json resolveRefString(const std::string& ref) const;
};

} // namespace agent
