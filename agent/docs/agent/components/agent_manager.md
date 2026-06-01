# AgentManager

`include/agent/agent_manager.hpp` · `src/agent/agent_manager.cpp`

## Overview

`AgentManager` is the top-level orchestrator for the agent engine. It owns all shared infrastructure and manages the full lifecycle of every agent. All public methods are thread-safe.

One `AgentManager` typically lives for the duration of the process. It is constructed once with a `Config`, an `LLMClient`, and an optional `MemoryBackend`, then consumed by C++ callers or wrapped by the [C ABI](c_api.md).

## Config

```cpp
struct Config {
    std::filesystem::path prompts_dir{"./prompts"};
    int                   thread_pool_size{16};
    int                   max_agent_depth{3};
    std::string           default_user_id{"default"};
};
```

| Field | Default | Meaning |
|---|---|---|
| `prompts_dir` | `"./prompts"` | Directory that `PromptLoader` reads template files from |
| `thread_pool_size` | `16` | Number of worker threads in the shared `ThreadPool` |
| `max_agent_depth` | `3` | Maximum sub-agent nesting depth enforced by `TaskAction` |
| `default_user_id` | `"default"` | User id used when none is provided in an agent config |

## Construction

```cpp
explicit AgentManager(Config                         config,
                      std::shared_ptr<LLMClient>     llm,
                      std::shared_ptr<MemoryBackend> memory = nullptr);
```

- `llm` is shared across all agents spawned by this manager.
- `memory` defaults to `NoOpMemoryBackend` when `nullptr`.
- The constructor calls `registerBuiltinItems()` which registers all built-in `WorkItem` types with the `WorkFactory`.

## Infrastructure Accessors

```cpp
WorkFactory&   factory();
PromptLoader&  promptLoader();
EventBus&      eventBus();
Blackboard&    blackboard();
ThreadPool&    threadPool();
QuotaManager&  quotaManager();
```

Return references to the shared infrastructure objects owned by the manager. Safe to use across threads.

## Agent Lifecycle

### Spawning

```cpp
std::string spawnAgent(const AgentConfig& cfg);
```

Creates an `Agent` with its own `AgentContext` and `MessageInbox`, stores the entry in the agent registry, and returns the assigned `agent_id`. Does not start the agent loop. The caller must call `runAgent` to start execution.

Enforces per-user quota via `QuotaManager::tryAcquireAgent`. Throws if the user is at their agent limit.

### Running

```cpp
std::future<nlohmann::json> runAgent(const std::string& agent_id,
                                      const std::string& task);
nlohmann::json runAgentBlocking(const std::string& agent_id,
                                const std::string& task);
```

`runAgent` starts the agent loop on a **dedicated `std::thread`** (not the shared pool) and returns a `std::future` for the final output. `runAgentBlocking` calls `runAgent` and waits for the future.

Each agent loop runs on its own dedicated thread (L2 concurrency), keeping the shared `ThreadPool` free for intra-batch parallelism (L3) and preventing deadlocks where agent loops wait on futures they themselves submitted to the pool.

### Status and Cancellation

```cpp
nlohmann::json listAgents(const std::string& user_id = "") const;
nlohmann::json getStatus(const std::string& agent_id) const;
void           cancelAgent(const std::string& agent_id);
void           destroyAgent(const std::string& agent_id);
```

`cancelAgent` sets `AgentContext::cancellation_flag` atomically; the agent stops after its current batch. `destroyAgent` joins the runner thread and removes the entry from the registry.

## Pattern A — Delegation / Piping

```cpp
void pipe(const std::string& from_id,
          const std::string& to_id,
          const std::string& template_string);
```

Registers a pipe: when `from_id` finishes, `template_string` is applied to its output (substituting `{prev_output}`) and the result is used as the task for `to_id`. `TaskAction` uses `runAgentBlocking` to implement synchronous sub-agent delegation.

## Pattern B — Messaging

```cpp
void                  sendMessage(const std::string& from_id,
                                  const std::string& to_id,
                                  const nlohmann::json& msg);
void                  broadcast(const std::string& from_id,
                                const nlohmann::json& msg);
std::vector<Message>  drainInbox(const std::string& agent_id);
```

Messages are placed into the destination agent's `MessageInbox`. `broadcast` delivers to every other registered agent. `drainInbox` atomically removes and returns all queued messages.

## Pattern C — Blackboard

```cpp
void                     blackboardWrite(const std::string& key, nlohmann::json v);
nlohmann::json           blackboardRead(const std::string& key);
std::vector<std::string> blackboardKeys(const std::string& prefix = "");
void                     blackboardDelete(const std::string& key);
```

Delegates to the shared `Blackboard`. Each agent also accesses it directly via `ctx.blackboard()`.

## Fan-Out / Fan-In

```cpp
std::vector<std::future<nlohmann::json>> fanOut(
    const std::vector<AgentConfig>& configs,
    const std::string&              shared_task);

nlohmann::json fanIn(
    std::vector<std::future<nlohmann::json>>& futures,
    const AgentConfig&                        synthesizer_config);

nlohmann::json researchFromAngles(
    const std::vector<std::string>& angles,
    const std::string&              topic);
```

`fanOut` spawns N agents with a common task and returns their futures. `fanIn` waits for all futures and passes the combined results to a synthesiser agent. `researchFromAngles` combines both into a single call.

## Real-Time Injection

```cpp
void injectWork(const std::string& agent_id, std::unique_ptr<WorkItem> item);
```

Thread-safe injection into a running agent's queue. Calls `AgentContext::injectFromOutside`, which acquires the queue mutex and notifies the blocking `pop()`.

## Event Subscription

```cpp
void subscribeEvents(EventCallback cb, void* key = nullptr);
void unsubscribeEvents(void* key);
```

Delegates to the shared `EventBus`. The `key` pointer is used for unsubscription. Callbacks fire on engine threads; GUI consumers must marshal to their UI thread.

## Prompt Hot Reload

```cpp
void reloadPrompts();
void setPromptsDir(const std::filesystem::path& dir);
```

Drops the `PromptLoader` cache. The next `render()` call re-reads template files from disk. Useful for development without restarting the process.

## Multi-Tenancy

```cpp
void setUserQuota(const std::string& user_id, const UserQuota& quota);
```

Configures per-user resource limits via `QuotaManager`. Creates the user record if absent.

## MCP Management

```cpp
struct MCPServerConfig {
    std::string    name;
    std::string    url;
    nlohmann::json extra;
};

void           connectMCP(const MCPServerConfig& cfg);
void           disconnectMCP(const std::string& server_name);
nlohmann::json listMCPServers() const;
```

Registers MCP servers by name for use by `MCPToolAction`. Connection logic is a stub in v1; implement the JSON-RPC wiring in `c_api.cpp` to enable real calls.

## Thread-Safety

All public methods are safe to call concurrently. State is partitioned:

| State | Guard |
|---|---|
| Agent registry (`m_agents`) | `m_agents_mutex` |
| Pipe registry (`m_pipes`) | `m_pipes_mutex` |
| MCP registry (`m_mcp_servers`) | `m_mcp_mutex` |
| Pool, blackboard, event bus, prompt loader | Each component's own mutex |

## Related Components

- [`Agent`](agent.md) — runs the per-agent event loop
- [`AgentContext`](agent_context.md) — per-agent runtime state
- [`ThreadPool`](thread_pool.md) — shared L3 worker pool
- [`WorkFactory`](work_factory.md) — item type registry
- [`QuotaManager`](quota_manager.md) — per-user resource limits
- [`LLMClient`](llm_client.md) — LLM call interface
- [`MemoryBackend`](memory_backend.md) — long-term memory interface
- [`C ABI`](c_api.md) — `am_create`, `am_spawn_agent`, all `am_*` functions
