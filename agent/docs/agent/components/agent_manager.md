# AgentManager

`include/agent/agent_manager.hpp` · `src/agent/agent_manager.cpp`

## Overview

`AgentManager` is the central orchestrator of the agent engine. It owns every piece of shared infrastructure (thread pool, blackboard, event bus, prompt loader, quota manager, work factory), manages the full lifecycle of agents, and exposes multi-agent orchestration patterns A, B, and C.

All public methods are safe to call concurrently from any thread. Internal state is protected by four independent mutexes — agents, pipes, MCP servers, and quota — so unrelated operations never contend.

---

## Configuration

```cpp
struct AgentManager::Config {
    std::filesystem::path prompts_dir{"./prompts"};
    int                   thread_pool_size{16};
    int                   max_agent_depth{3};
    std::string           default_user_id{"default"};
};
```

| Field | Default | Description |
|---|---|---|
| `prompts_dir` | `./prompts` | Directory scanned by `PromptLoader` for `*.md` templates |
| `thread_pool_size` | `16` | Number of worker threads in the shared `ThreadPool` |
| `max_agent_depth` | `3` | Maximum sub-agent nesting depth before `TaskAction` refuses to spawn |
| `default_user_id` | `"default"` | User id used when a spawn config does not specify one |

---

## Construction

```cpp
AgentManager(Config config,
             std::shared_ptr<LLMClient>     llm,
             std::shared_ptr<MemoryBackend> memory = nullptr);
```

Constructs and starts:
- `WorkFactory` — registers all built-in stages and actions
- `PromptLoader` — caches templates from `config.prompts_dir`
- `EventBus` — synchronous pub/sub bus
- `Blackboard` — shared key-value store (wired to the event bus)
- `ThreadPool` — `config.thread_pool_size` worker threads
- `QuotaManager` — starts empty; per-user limits default to `UserQuota{}`

`AgentManager` is non-copyable and non-movable.

---

## Infrastructure Accessors

```cpp
WorkFactory&   factory();
PromptLoader&  promptLoader();
EventBus&      eventBus();
Blackboard&    blackboard();
ThreadPool&    threadPool();
QuotaManager&  quotaManager();
```

Return references to the shared subsystems. Useful for tests or advanced consumers that need direct access.

---

## Agent Lifecycle

### `spawnAgent`

```cpp
std::string spawnAgent(const AgentConfig& cfg);
```

Creates a new `AgentEntry` (agent + inbox) and stores it in the registry. Returns the generated agent id (e.g. `"agent_1"`). Does **not** start the agent loop; call `runAgent` or `runAgentBlocking` to start it.

Quota check: calls `QuotaManager::tryAcquireAgent` for `cfg.extra["user_id"]`. Throws `std::runtime_error("quota exceeded")` if the user is at their agent limit.

### `runAgent`

```cpp
std::future<nlohmann::json> runAgent(const std::string& agent_id,
                                     const std::string& task);
```

Starts the agent loop on a **dedicated `std::thread`** (not a pool thread — see [Concurrency levels](../concurrency.md)). Returns a `std::future<json>` that resolves to the agent's final output JSON when the loop terminates.

The dedicated thread prevents pool reentrancy deadlock: an agent loop may submit batch items to the shared pool without risk of deadlocking against its own execution.

### `runAgentBlocking`

```cpp
nlohmann::json runAgentBlocking(const std::string& agent_id,
                                const std::string& task);
```

Calls `runAgent` then immediately blocks until the future resolves. Convenient for synchronous orchestration flows.

### `destroyAgent`

```cpp
void destroyAgent(const std::string& agent_id);
```

Cancels the agent (sets `cancellation_flag`), joins the dedicated runner thread, then erases the entry from the registry.

### `cancelAgent`

```cpp
void cancelAgent(const std::string& agent_id);
```

Sets `AgentContext::cancellation_flag`. The agent stops after the current batch finishes. Does not join the thread.

### `getStatus`

```cpp
nlohmann::json getStatus(const std::string& agent_id) const;
```

Returns `{"id", "name", "status", "user_id", "iterations", "result?"}`. Status values: `"idle"`, `"running"`, `"done"`, `"failed"`.

### `listAgents`

```cpp
nlohmann::json listAgents(const std::string& user_id = "") const;
```

Returns a JSON array of status objects. Pass `user_id=""` for an admin view of all agents.

---

## Pattern A — Delegation / Pipe

```cpp
void pipe(const std::string& from_id,
          const std::string& to_id,
          const std::string& template_string);
```

Registers a pipe: when `from_id` finishes, `template_string` (which may reference `{prev_output}`) is applied to the output, and the result is sent as the task to `to_id`. Pipes are stored in `m_pipes` and evaluated inside `onAgentFinished`.

---

## Pattern B — Messaging

```cpp
void                  sendMessage(const std::string& from_id,
                                  const std::string& to_id,
                                  const nlohmann::json& msg);

void                  broadcast(const std::string& from_id,
                                const nlohmann::json& msg);

std::vector<Message>  drainInbox(const std::string& agent_id);
```

`sendMessage` pushes a `Message` into `to_id`'s `MessageInbox`.  
`broadcast` delivers to every agent *except* `from_id`.  
`drainInbox` removes and returns all queued messages for `agent_id`.

---

## Pattern C — Blackboard

```cpp
void                     blackboardWrite(const std::string& key, nlohmann::json v);
nlohmann::json           blackboardRead(const std::string& key);
std::vector<std::string> blackboardKeys(const std::string& prefix = "");
void                     blackboardDelete(const std::string& key);
```

Convenience wrappers that delegate to the shared `Blackboard`. Agents can also access the blackboard directly through `AgentContext::blackboard()`.

---

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

`fanOut` spawns N agents (one per config) all given the same `shared_task`, starts them all, and returns their futures.

`fanIn` waits for all futures, collects their outputs, and runs a final synthesiser agent whose task is a summary of all individual results. The synthesiser's output is returned.

`researchFromAngles` is a convenience composition: spawns one researcher per angle, then a single synthesiser. Agents share results via the Blackboard.

---

## Real-Time Injection

```cpp
void injectWork(const std::string& agent_id, std::unique_ptr<WorkItem> item);
```

Thread-safe push into the named agent's queue at the **front** (high priority). The running batch is not interrupted; the injected item appears at the next batch boundary.

---

## Event Subscription

```cpp
void subscribeEvents(EventCallback cb, void* key = nullptr);
void unsubscribeEvents(void* key);
```

Delegates to `EventBus`. Callbacks fire on engine threads — GUI consumers must marshal to their UI thread.

---

## Prompt Hot Reload

```cpp
void reloadPrompts();
void setPromptsDir(const std::filesystem::path& dir);
```

`reloadPrompts` drops the `PromptLoader` cache; templates are re-read from disk on next use.  
`setPromptsDir` changes the directory and implies a reload.

---

## Multi-Tenancy / Quota

```cpp
void setUserQuota(const std::string& user_id, const UserQuota& quota);
```

Configures per-user resource limits. See [`QuotaManager`](quota_manager.md).

---

## MCP Management

```cpp
void           connectMCP(const MCPServerConfig& cfg);
void           disconnectMCP(const std::string& server_name);
nlohmann::json listMCPServers() const;
```

Stores MCP server registrations in `m_mcp_servers`. `MCPToolAction` looks up the active connection when dispatching tool calls.

```cpp
struct MCPServerConfig {
    std::string    name;
    std::string    url;
    nlohmann::json extra;
};
```

---

## Internal `AgentEntry`

Each spawned agent is stored as an `AgentEntry`:

```cpp
struct AgentEntry {
    AgentConfig                   config;
    std::unique_ptr<Agent>        agent;
    std::unique_ptr<MessageInbox> inbox;
    std::string                   status;   // "idle"|"running"|"done"|"failed"
    nlohmann::json                result;
    std::string                   user_id;
    std::thread                   runner;   // dedicated loop thread
};
```

The dedicated `runner` thread is the key design choice that prevents pool reentrancy deadlock. See [Concurrency levels](../concurrency.md) for the full explanation.

---

## Thread-Safety

| Resource | Guard |
|---|---|
| Agent registry `m_agents` | `m_agents_mutex` |
| Pipe registry `m_pipes` | `m_pipes_mutex` |
| MCP registry `m_mcp_servers` | `m_mcp_mutex` |
| Quota state | `QuotaManager::m_mutex` |
| Blackboard | `Blackboard::m_mutex` |
| EventBus | `EventBus::m_mutex` |
| PromptLoader cache | `PromptLoader::m_mutex` (shared) |
| WorkFactory registry | `WorkFactory::m_mutex` (shared) |

---

## Destructor

`~AgentManager()` cancels every running agent, gathers all `runner` threads, and joins them before tearing down the thread pool. This guarantees no thread escapes its lifetime.

---

## Related Components

- [`Agent`](agent.md) — the loop logic
- [`AgentContext`](agent.md) — per-agent runtime state
- [`ThreadPool`](thread_pool.md) — L3 parallelism
- [`BatchExecutor`](batch_executor.md) — DAG scheduling
- [`QuotaManager`](quota_manager.md) — per-user limits
- [`Blackboard`](blackboard.md) — Pattern C
- [`MessageInbox`](message_inbox.md) — Pattern B
- [`EventBus`](event_bus.md) — event notifications
- [C ABI](c_api.md) — exposes all of the above over a stable C interface
