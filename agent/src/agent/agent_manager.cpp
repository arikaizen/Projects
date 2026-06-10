// agent_manager.cpp — AgentManager implementation
#include "agent/agent_manager.hpp"
#include "agent/llm_factory.hpp"
#include "agent/memory_backend.hpp"
#include "actions/mcp_tool_action.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef AGENT_HAS_MCP_HTTP
#  include <httplib.h>
#endif

namespace agent {

// ---------------------------------------------------------------------------
// Internal helper — ISO-8601 UTC "now"
// ---------------------------------------------------------------------------
static std::string nowISO8601()
{
    auto tp   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_utc{};
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
// Forward declarations for built-in item registration functions.
// Defined in the stage/action translation units; linked into agent_core.
// (Already inside namespace agent — no wrapper needed.)
// ---------------------------------------------------------------------------
void registerReasonStage(WorkFactory&);
void registerInjectionStage(WorkFactory&);
void registerTransformStage(WorkFactory&);
void registerValidateStage(WorkFactory&);
void registerUnderstandStage(WorkFactory&);
void registerOrientStage(WorkFactory&);
void registerLocateStage(WorkFactory&);
void registerReadStage(WorkFactory&);
void registerCodeIntelStage(WorkFactory&);
void registerObserveStage(WorkFactory&);
void registerRespondStage(WorkFactory&);
void registerBashAction(WorkFactory&);
void registerReadAction(WorkFactory&);
void registerWriteAction(WorkFactory&);
void registerEditAction(WorkFactory&);
void registerGlobAction(WorkFactory&);
void registerGrepAction(WorkFactory&);
void registerWebFetchAction(WorkFactory&);
void registerWebSearchAction(WorkFactory&);
void registerTaskAction(WorkFactory&);
void registerTodoWriteAction(WorkFactory&);
void registerMemoryActions(WorkFactory&);
void registerMessagingActions(WorkFactory&);
void registerBlackboardActions(WorkFactory&);
// MCPToolAction is declared in mcp_tool_action.hpp (already included above)

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
AgentManager::AgentManager(Config                         config,
                            std::shared_ptr<LLMClient>     llm,
                            std::shared_ptr<MemoryBackend> memory)
    : m_config       (std::move(config))
    , m_llm          (std::move(llm))
    , m_memory       (memory ? std::move(memory)
                              : std::make_shared<NoOpMemoryBackend>())
    , m_factory      (std::make_shared<WorkFactory>())
    , m_prompt_loader(std::make_shared<PromptLoader>(m_config.prompts_dir))
    , m_event_bus    (std::make_shared<EventBus>())
    , m_blackboard   (std::make_shared<Blackboard>(m_event_bus.get()))
    , m_pool         (std::make_shared<ThreadPool>(
                          static_cast<std::size_t>(m_config.thread_pool_size)))
    , m_quota        (std::make_shared<QuotaManager>())
{
    registerBuiltinItems();
    std::cerr << "[AgentManager] initialised (pool=" << m_config.thread_pool_size
              << ", prompts=" << m_config.prompts_dir.string() << ")\n";
}

// ---------------------------------------------------------------------------
// Destructor — pool shuts down automatically via shared_ptr destruction
// ---------------------------------------------------------------------------
AgentManager::~AgentManager()
{
    // Cancel all running agents so their loops wake and exit.
    std::vector<std::string> ids;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        for (auto& [id, _] : m_agents) ids.push_back(id);
    }
    for (const auto& id : ids) {
        try { cancelAgent(id); } catch (...) {}
    }

    // Join every runner thread before tearing down the registry, so no loop is
    // left running against an Agent that is about to be destroyed.
    std::vector<std::thread> runners;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        for (auto& [id, entry] : m_agents)
            if (entry.runner.joinable()) runners.push_back(std::move(entry.runner));
    }
    for (auto& t : runners) {
        if (t.joinable()) t.join();
    }
}

// ---------------------------------------------------------------------------
// registerBuiltinItems
// ---------------------------------------------------------------------------
void AgentManager::registerBuiltinItems()
{
    // Core reasoning stages
    registerUnderstandStage(*m_factory);  // Step 1  — Understand the goal
    registerOrientStage(*m_factory);      // Step 2A — Orient
    registerLocateStage(*m_factory);      // Step 2B — Locate
    registerReadStage(*m_factory);        // Step 2C — Read
    registerValidateStage(*m_factory);    // Step 2D — Verify (optional, used in plans)
    registerCodeIntelStage(*m_factory);   // Step 2E — Code intelligence (optional)
    registerReasonStage(*m_factory);      // Step 3  — Reason & decide
    registerObserveStage(*m_factory);     // Step 5  — Observe
    registerRespondStage(*m_factory);     // Step 6  — Respond
    registerInjectionStage(*m_factory);
    registerTransformStage(*m_factory);

    registerBashAction(*m_factory);
    registerReadAction(*m_factory);
    registerWriteAction(*m_factory);
    registerEditAction(*m_factory);
    registerGlobAction(*m_factory);
    registerGrepAction(*m_factory);
    registerWebFetchAction(*m_factory);
    registerWebSearchAction(*m_factory);
    registerTaskAction(*m_factory);
    registerTodoWriteAction(*m_factory);
    registerMemoryActions(*m_factory);
    registerMessagingActions(*m_factory);
    registerBlackboardActions(*m_factory);
    registerMCPToolAction(*m_factory);
}

// ---------------------------------------------------------------------------
// makeContext — construct an AgentContext for an AgentEntry
// ---------------------------------------------------------------------------
std::unique_ptr<AgentContext> AgentManager::makeContext(const AgentEntry& entry)
{
    return std::make_unique<AgentContext>(
        entry.config,
        entry.llm ? entry.llm : defaultLLM(),
        m_factory,
        m_prompt_loader,
        m_memory,
        m_blackboard.get(),
        entry.inbox.get(),
        m_event_bus.get(),
        this);
}

// ---------------------------------------------------------------------------
// getEntry / getEntryConst — throws if absent
// ---------------------------------------------------------------------------
AgentManager::AgentEntry& AgentManager::getEntry(const std::string& agent_id)
{
    auto it = m_agents.find(agent_id);
    if (it == m_agents.end()) {
        throw std::runtime_error("AgentManager: unknown agent_id '" + agent_id + "'");
    }
    return it->second;
}

const AgentManager::AgentEntry& AgentManager::getEntryConst(const std::string& id) const
{
    auto it = m_agents.find(id);
    if (it == m_agents.end()) {
        throw std::runtime_error("AgentManager: unknown agent_id '" + id + "'");
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// spawnAgent
// ---------------------------------------------------------------------------
std::string AgentManager::spawnAgent(const AgentConfig& cfg)
{
    std::string user_id = (cfg.extra.is_object() && cfg.extra.contains("user_id"))
                             ? cfg.extra["user_id"].get<std::string>()
                             : m_config.default_user_id;

    // Enforce quota
    if (!m_quota->tryAcquireAgent(user_id)) {
        throw std::runtime_error("AgentManager: quota exceeded for user '" + user_id +
                                 "' (max_concurrent_agents=" +
                                 std::to_string(m_quota->getQuota(user_id).max_concurrent_agents) +
                                 ")");
    }

    std::string agent_id;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);

        agent_id = "agent_" + std::to_string(++m_agent_counter);

        AgentConfig resolved_cfg = cfg;
        resolved_cfg.agent_id    = agent_id;

        AgentEntry entry;
        entry.config  = resolved_cfg;
        entry.inbox   = std::make_unique<MessageInbox>();
        entry.status  = "idle";
        entry.user_id = user_id;

        // Per-agent LLM override: extra["llm"] selects a provider/model for
        // this agent only (mixed-provider agent groups). Falls back to the
        // manager default when absent or when the backend isn't compiled in.
        if (resolved_cfg.extra.is_object() && resolved_cfg.extra.contains("llm") &&
            resolved_cfg.extra["llm"].is_object()) {
            std::string llm_err;
            entry.llm = makeLLMClientFromConfig(resolved_cfg.extra["llm"], &llm_err);
            if (!entry.llm) {
                std::cerr << "[AgentManager] " << agent_id
                          << ": per-agent llm config rejected (" << llm_err
                          << ") — using manager default\n";
            }
        }

        // Build context and agent
        entry.agent = std::make_unique<Agent>(makeContext(entry), *m_pool);

        m_agents.emplace(agent_id, std::move(entry));
    }

    m_event_bus->emit(EventBus::makeEvent("agent_spawned",
                      {{"agent_id", agent_id},
                       {"user_id",  user_id}}));

    std::cerr << "[AgentManager] spawned " << agent_id << " for user " << user_id << "\n";
    return agent_id;
}

// ---------------------------------------------------------------------------
// destroyAgent
// ---------------------------------------------------------------------------
void AgentManager::destroyAgent(const std::string& agent_id)
{
    // Cancel first (no-op if already done) so a running loop wakes and exits.
    try { cancelAgent(agent_id); } catch (...) {}

    // Take the runner thread out under the lock, then join it OUTSIDE the lock
    // (the runner calls onAgentFinished, which locks m_agents_mutex).  We must
    // join before erasing the entry so the loop is no longer using its Agent.
    std::thread runner;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto it = m_agents.find(agent_id);
        if (it == m_agents.end()) return;
        runner = std::move(it->second.runner);
    }
    if (runner.joinable()) runner.join();

    std::string user_id;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto it = m_agents.find(agent_id);
        if (it == m_agents.end()) return;
        user_id = it->second.user_id;
        m_agents.erase(it);
    }

    m_quota->releaseAgent(user_id);

    m_event_bus->emit(EventBus::makeEvent("agent_destroyed",
                      {{"agent_id", agent_id}}));

    std::cerr << "[AgentManager] destroyed " << agent_id << "\n";
}

// ---------------------------------------------------------------------------
// runAgent — non-blocking; returns a future<json>
// ---------------------------------------------------------------------------
std::future<nlohmann::json> AgentManager::runAgent(const std::string& agent_id,
                                                     const std::string& task)
{
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto& entry   = getEntry(agent_id);
        entry.status  = "running";
        if (!task.empty()) {
            entry.config.task = task;
        }
        // Kick off the agent loop.  If the caller already seeded the queue
        // (e.g. via injectWork before runAgent — the common path in tests and
        // for delegated sub-tasks), respect that work and do not inject a
        // ReasonStage.  Otherwise, start the agent by reasoning about its task:
        // push an initial ReasonStage that renders the task prompt, calls the
        // LLM, and pushes whatever plan items the LLM returns.
        if (entry.agent->context().queueEmpty()) {
            // Start with UnderstandStage (full 6-step loop) when available,
            // otherwise fall back to ReasonStage for lightweight/test setups.
            if (m_factory->isRegistered("UnderstandStage")) {
                auto initial = m_factory->create("UnderstandStage", "init_understand",
                                                 nlohmann::json{{"task", entry.config.task}});
                entry.agent->context().push(std::move(initial),
                                            AgentContext::Position::Back);
            } else if (m_factory->isRegistered("ReasonStage")) {
                auto initial = m_factory->create("ReasonStage", "init_reason",
                                                 nlohmann::json{{"task", entry.config.task}});
                entry.agent->context().push(std::move(initial),
                                            AgentContext::Position::Back);
            }
        }
    }

    // Take ownership of any prior runner thread and join it outside the lock
    // (a re-run of the same agent must not assign over a joinable std::thread).
    std::thread old_runner;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        old_runner = std::move(getEntry(agent_id).runner);
    }
    if (old_runner.joinable()) old_runner.join();

    // Run the agent loop on its OWN dedicated thread (not the shared pool).
    // The pool is reserved for intra-batch parallelism; keeping agent loops off
    // it prevents a reentrancy deadlock where pool-resident agent loops block
    // waiting for batch items that cannot be scheduled on a saturated pool.
    auto prom = std::make_shared<std::promise<nlohmann::json>>();
    std::future<nlohmann::json> fut = prom->get_future();

    std::thread runner([this, agent_id, prom]() {
        Agent* agent_ptr = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_agents_mutex);
            auto it = m_agents.find(agent_id);
            if (it == m_agents.end()) {
                prom->set_value({{"error", "agent not found"}});
                return;
            }
            agent_ptr = it->second.agent.get();
        }

        nlohmann::json output;
        try {
            auto result = agent_ptr->run();
            output = {
                {"reason",     Agent::reasonToString(result.reason)},
                {"output",     result.output},
                {"iterations", result.iterations},
                {"error",      result.error}
            };
            onAgentFinished(agent_id, output);
        } catch (const std::exception& ex) {
            output = {{"error", ex.what()}};
            onAgentFinished(agent_id, output);
        }
        prom->set_value(output);
    });

    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto it = m_agents.find(agent_id);
        if (it != m_agents.end()) it->second.runner = std::move(runner);
        else runner.detach();  // agent destroyed already; let the thread finish
    }
    return fut;
}

// ---------------------------------------------------------------------------
// runAgentBlocking
// ---------------------------------------------------------------------------
nlohmann::json AgentManager::runAgentBlocking(const std::string& agent_id,
                                               const std::string& task)
{
    auto fut = runAgent(agent_id, task);
    return fut.get();
}

// ---------------------------------------------------------------------------
// onAgentFinished — called by the worker thread after run() returns
// ---------------------------------------------------------------------------
void AgentManager::onAgentFinished(const std::string&    agent_id,
                                    const nlohmann::json& result)
{
    std::string user_id;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto it = m_agents.find(agent_id);
        if (it == m_agents.end()) return;

        auto& entry  = it->second;
        bool has_error = result.contains("error")
                         && result["error"].is_string()
                         && !result["error"].get<std::string>().empty();
        entry.status = has_error ? "failed" : "done";
        entry.result = result;
        user_id      = entry.user_id;
    }

    m_event_bus->emit(EventBus::makeEvent("agent_finished",
                      {{"agent_id", agent_id},
                       {"result",   result}}));

    // Check pipe registry — if any pipe FROM this agent, apply and inject to
    // destination agents.
    std::vector<PipeEntry> matching_pipes;
    {
        std::unique_lock<std::mutex> lock(m_pipes_mutex);
        for (const auto& p : m_pipes) {
            if (p.from_id == agent_id) {
                matching_pipes.push_back(p);
            }
        }
    }

    for (const auto& p : matching_pipes) {
        try {
            // Simple template: replace {{OUTPUT}} with the JSON result string
            std::string rendered = p.template_str;
            const std::string placeholder = "{{OUTPUT}}";
            std::size_t pos = rendered.find(placeholder);
            if (pos != std::string::npos) {
                rendered.replace(pos, placeholder.size(), result.dump());
            }

            // Send the rendered message to the destination agent
            nlohmann::json msg = {
                {"type",    "pipe"},
                {"from",    agent_id},
                {"content", rendered}
            };
            sendMessage(agent_id, p.to_id, msg);
        } catch (const std::exception& ex) {
            std::cerr << "[AgentManager] pipe error from " << agent_id
                      << " to " << p.to_id << ": " << ex.what() << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// cancelAgent
// ---------------------------------------------------------------------------
void AgentManager::cancelAgent(const std::string& agent_id)
{
    std::unique_lock<std::mutex> lock(m_agents_mutex);
    auto it = m_agents.find(agent_id);
    if (it == m_agents.end()) return;

    auto& entry = it->second;
    entry.agent->context().cancellation_flag = true;
    entry.agent->context().wakeLoop();
    entry.status = "cancelled";

    std::cerr << "[AgentManager] cancel requested for " << agent_id << "\n";
}

// ---------------------------------------------------------------------------
// listAgents
// ---------------------------------------------------------------------------
nlohmann::json AgentManager::listAgents(const std::string& user_id) const
{
    std::unique_lock<std::mutex> lock(m_agents_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [id, entry] : m_agents) {
        if (!user_id.empty() && entry.user_id != user_id) continue;
        arr.push_back({
            {"agent_id", id},
            {"name",     entry.config.name},
            {"status",   entry.status},
            {"user_id",  entry.user_id},
            {"task",     entry.config.task}
        });
    }
    return arr;
}

// ---------------------------------------------------------------------------
// getStatus
// ---------------------------------------------------------------------------
nlohmann::json AgentManager::getStatus(const std::string& agent_id) const
{
    std::unique_lock<std::mutex> lock(m_agents_mutex);
    const auto& entry = getEntryConst(agent_id);
    return {
        {"agent_id",    agent_id},
        {"name",        entry.config.name},
        {"status",      entry.status},
        {"user_id",     entry.user_id},
        {"task",        entry.config.task},
        {"result",      entry.result},
        {"iteration",   entry.agent->context().iteration_count},
        {"queue_size",  static_cast<int>(entry.agent->context().queueSize())}
    };
}

// ---------------------------------------------------------------------------
// Pattern A — pipe
// ---------------------------------------------------------------------------
void AgentManager::pipe(const std::string& from_id,
                         const std::string& to_id,
                         const std::string& template_string)
{
    std::unique_lock<std::mutex> lock(m_pipes_mutex);
    m_pipes.push_back({from_id, to_id, template_string});
    std::cerr << "[AgentManager] pipe registered: " << from_id << " -> " << to_id << "\n";
}

// ---------------------------------------------------------------------------
// Pattern B — messaging
// ---------------------------------------------------------------------------
void AgentManager::sendMessage(const std::string&    from_id,
                                const std::string&    to_id,
                                const nlohmann::json& msg)
{
    Message m;
    m.from_id   = from_id;
    m.to_id     = to_id;
    m.payload   = msg;
    m.timestamp = nowISO8601();

    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        auto& entry = getEntry(to_id);
        entry.inbox->push(m);
    }

    m_event_bus->emit(EventBus::makeEvent("message_sent",
                      {{"from", from_id}, {"to", to_id}}));
}

void AgentManager::broadcast(const std::string& from_id, const nlohmann::json& msg)
{
    std::vector<std::string> targets;
    {
        std::unique_lock<std::mutex> lock(m_agents_mutex);
        for (const auto& [id, _] : m_agents) {
            if (id != from_id) targets.push_back(id);
        }
    }
    for (const auto& id : targets) {
        try { sendMessage(from_id, id, msg); } catch (...) {}
    }
}

std::vector<Message> AgentManager::drainInbox(const std::string& agent_id)
{
    std::unique_lock<std::mutex> lock(m_agents_mutex);
    auto& entry = getEntry(agent_id);
    return entry.inbox->drain();
}

// ---------------------------------------------------------------------------
// Pattern C — blackboard delegation
// ---------------------------------------------------------------------------
void AgentManager::blackboardWrite(const std::string& key, nlohmann::json v)
{
    m_blackboard->write(key, std::move(v));
}

nlohmann::json AgentManager::blackboardRead(const std::string& key)
{
    auto opt = m_blackboard->read(key);
    if (!opt) throw std::runtime_error("Blackboard key not found: " + key);
    return *opt;
}

std::vector<std::string> AgentManager::blackboardKeys(const std::string& prefix)
{
    return m_blackboard->keys(prefix);
}

void AgentManager::blackboardDelete(const std::string& key)
{
    m_blackboard->remove(key);
}

// ---------------------------------------------------------------------------
// Composition — fan-out / fan-in / researchFromAngles
// ---------------------------------------------------------------------------
std::vector<std::future<nlohmann::json>> AgentManager::fanOut(
    const std::vector<AgentConfig>& configs,
    const std::string&              shared_task)
{
    std::vector<std::future<nlohmann::json>> futures;
    futures.reserve(configs.size());

    for (const auto& cfg : configs) {
        std::string id = spawnAgent(cfg);
        futures.push_back(runAgent(id, shared_task));
    }

    return futures;
}

nlohmann::json AgentManager::fanIn(
    std::vector<std::future<nlohmann::json>>& futures,
    const AgentConfig&                        synthesizer_config)
{
    // Collect all fan-out results onto the blackboard
    nlohmann::json findings = nlohmann::json::array();
    for (std::size_t i = 0; i < futures.size(); ++i) {
        try {
            auto res = futures[i].get();
            findings.push_back(res);
            m_blackboard->write("fanout_result_" + std::to_string(i), res);
        } catch (const std::exception& ex) {
            findings.push_back({{"error", ex.what()}});
        }
    }

    m_blackboard->write("fanout_findings", findings);

    // Spawn and run the synthesizer
    std::string synth_id = spawnAgent(synthesizer_config);
    return runAgentBlocking(synth_id,
                             synthesizer_config.task.empty()
                                 ? "Synthesize findings from blackboard key 'fanout_findings'"
                                 : synthesizer_config.task);
}

nlohmann::json AgentManager::researchFromAngles(
    const std::vector<std::string>& angles,
    const std::string&              topic)
{
    std::vector<AgentConfig> configs;
    configs.reserve(angles.size());

    for (const auto& angle : angles) {
        AgentConfig cfg;
        cfg.name = "researcher_" + angle;
        cfg.task = "Research the following topic from the perspective of '" +
                   angle + "': " + topic;
        cfg.extra["user_id"] = m_config.default_user_id;
        configs.push_back(cfg);
    }

    auto futures = fanOut(configs, "");   // tasks are already in config

    AgentConfig synth;
    synth.name = "synthesizer";
    synth.task = "Synthesize all research findings about: " + topic;
    synth.extra["user_id"] = m_config.default_user_id;

    return fanIn(futures, synth);
}

// ---------------------------------------------------------------------------
// Real-time injection
// ---------------------------------------------------------------------------
void AgentManager::injectWork(const std::string&          agent_id,
                               std::unique_ptr<WorkItem>   item)
{
    std::unique_lock<std::mutex> lock(m_agents_mutex);
    auto& entry = getEntry(agent_id);
    entry.agent->context().injectFromOutside(std::move(item),
                                              AgentContext::Position::Front);
}

// ---------------------------------------------------------------------------
// Event subscription
// ---------------------------------------------------------------------------
void AgentManager::subscribeEvents(EventCallback cb, void* key)
{
    m_event_bus->subscribe(std::move(cb), key);
}

void AgentManager::unsubscribeEvents(void* key)
{
    m_event_bus->unsubscribe(key);
}

// ---------------------------------------------------------------------------
// Prompt hot reload
// ---------------------------------------------------------------------------
void AgentManager::reloadPrompts()
{
    m_prompt_loader->reload();
    m_event_bus->emit(EventBus::makeEvent("prompts_reloaded", {}));
    std::cerr << "[AgentManager] prompts reloaded\n";
}

void AgentManager::setPromptsDir(const std::filesystem::path& dir)
{
    m_prompt_loader->setPromptsDir(dir);
    m_event_bus->emit(EventBus::makeEvent("prompts_dir_changed",
                      {{"dir", dir.string()}}));
    std::cerr << "[AgentManager] prompts dir -> " << dir.string() << "\n";
}

// ---------------------------------------------------------------------------
// Multi-tenancy
// ---------------------------------------------------------------------------
void AgentManager::setUserQuota(const std::string& user_id, const UserQuota& quota)
{
    m_quota->setQuota(user_id, quota);
}

// ---------------------------------------------------------------------------
// LLM management
// ---------------------------------------------------------------------------
void AgentManager::setDefaultLLM(std::shared_ptr<LLMClient> llm)
{
    if (!llm) throw std::invalid_argument("AgentManager::setDefaultLLM: null client");
    std::string model_name = llm->modelName();
    {
        std::unique_lock<std::mutex> lock(m_llm_mutex);
        m_llm = std::move(llm);
    }
    m_event_bus->emit(EventBus::makeEvent("llm_changed", {{"model", model_name}}));
    std::cerr << "[AgentManager] default LLM -> " << model_name << "\n";
}

std::shared_ptr<LLMClient> AgentManager::defaultLLM() const
{
    std::unique_lock<std::mutex> lock(m_llm_mutex);
    return m_llm;
}

// ---------------------------------------------------------------------------
// MCP management
// ---------------------------------------------------------------------------
void AgentManager::connectMCP(const MCPServerConfig& cfg)
{
    {
        std::unique_lock<std::mutex> lock(m_mcp_mutex);
        m_mcp_servers[cfg.name] = cfg;
    }
    m_event_bus->emit(EventBus::makeEvent("mcp_connected",
                      {{"server", cfg.name}, {"url", cfg.url}}));
    std::cerr << "[AgentManager] MCP connected: " << cfg.name
              << " @ " << cfg.url << "\n";

#ifdef AGENT_HAS_MCP_HTTP
    if (cfg.transport != "stdio" && !cfg.url.empty()) {
        try {
            // Split "scheme://host[:port][/prefix]" into the origin httplib's
            // URL-based Client constructor wants (it selects http vs https from
            // the scheme, handling SSL internally when compiled with OpenSSL)
            // and the path prefix.
            auto scheme_end = cfg.url.find("://");
            if (scheme_end == std::string::npos)
                throw std::runtime_error("malformed URL: " + cfg.url);

            auto path_start = cfg.url.find('/', scheme_end + 3);
            std::string origin =
                (path_start == std::string::npos) ? cfg.url : cfg.url.substr(0, path_start);
            std::string path_prefix =
                (path_start == std::string::npos) ? "" : cfg.url.substr(path_start);
            while (!path_prefix.empty() && path_prefix.back() == '/')
                path_prefix.pop_back();

            // Build JSON-RPC 2.0 tools/list request
            nlohmann::json rpc_req = {
                {"jsonrpc", "2.0"},
                {"id",      "list-1"},
                {"method",  "tools/list"},
                {"params",  nlohmann::json::object()}
            };

            httplib::Headers headers;
            if (!cfg.bearer_token.empty())
                headers.emplace("Authorization", "Bearer " + cfg.bearer_token);
            headers.emplace("Content-Type", "application/json");

            httplib::Client cli(origin);
            cli.set_connection_timeout(5);
            cli.set_read_timeout(10);
            cli.set_follow_location(true);

            std::string endpoint = path_prefix + "/mcp/v1";
            auto res = cli.Post(endpoint.c_str(), headers, rpc_req.dump(), "application/json");
            if (!res) {
                std::cerr << "[AgentManager] MCP tools/list failed for '" << cfg.name
                          << "': " << httplib::to_string(res.error()) << "\n";
                return;
            }
            if (res->status < 200 || res->status >= 300) {
                std::cerr << "[AgentManager] MCP tools/list HTTP " << res->status
                          << " for '" << cfg.name << "'\n";
                return;
            }

            auto resp = nlohmann::json::parse(res->body);

            // Accept both {"result": [...]} and {"result": {"tools": [...]}}
            nlohmann::json tools_array = nlohmann::json::array();
            if (resp.contains("result")) {
                auto& r = resp["result"];
                if (r.is_array())
                    tools_array = r;
                else if (r.is_object() && r.contains("tools") && r["tools"].is_array())
                    tools_array = r["tools"];
            }

            int registered = 0;
            for (const auto& tool : tools_array) {
                if (!tool.contains("name") || !tool["name"].is_string()) continue;

                std::string tool_name = tool["name"].get<std::string>();
                std::string desc      = tool.value("description", "MCP tool: " + tool_name);
                nlohmann::json schema = tool.value("inputSchema", nlohmann::json::object());

                WorkItemSpec spec{ tool_name, desc, WorkItem::Kind::Action, schema };
                std::string server_name = cfg.name;

                m_factory->registerItem(std::move(spec),
                    [tool_name, server_name](std::string id, nlohmann::json inputs) {
                        return std::make_unique<MCPToolAction>(
                            std::move(id), tool_name, server_name, std::move(inputs));
                    });
                ++registered;
            }

            std::cerr << "[AgentManager] '" << cfg.name << "': registered "
                      << registered << " MCP tools\n";

        } catch (const std::exception& ex) {
            std::cerr << "[AgentManager] connectMCP tool enumeration error: "
                      << ex.what() << "\n";
        }
    }
#endif
}

void AgentManager::disconnectMCP(const std::string& server_name)
{
    {
        std::unique_lock<std::mutex> lock(m_mcp_mutex);
        m_mcp_servers.erase(server_name);
    }
    m_event_bus->emit(EventBus::makeEvent("mcp_disconnected",
                      {{"server", server_name}}));
    std::cerr << "[AgentManager] MCP disconnected: " << server_name << "\n";
}

nlohmann::json AgentManager::listMCPServers() const
{
    std::unique_lock<std::mutex> lock(m_mcp_mutex);
    // Returns an object keyed by server name so callers can do servers[name].
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& [name, cfg] : m_mcp_servers) {
        obj[name] = {
            {"name",          cfg.name},
            {"url",           cfg.url},
            {"bearer_token",  cfg.bearer_token},
            {"transport",     cfg.transport},
            {"extra",         cfg.extra},
        };
    }
    return obj;
}

} // namespace agent
