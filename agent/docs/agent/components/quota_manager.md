# QuotaManager

`include/agent/quota.hpp` · `src/agent/quota.cpp`

## Overview

`QuotaManager` enforces per-user resource limits for three resource types: concurrent agents, concurrent LLM calls, and concurrent tool (action) executions. It is shared across all agents spawned under the same `AgentManager`.

## `UserQuota`

```cpp
struct UserQuota {
    int max_concurrent_agents{10};
    int max_llm_inflight{5};
    int max_tool_inflight{20};
};
```

| Field | Default | Meaning |
|---|---|---|
| `max_concurrent_agents` | `10` | Max agents running concurrently for this user |
| `max_llm_inflight` | `5` | Max concurrent LLM calls |
| `max_tool_inflight` | `20` | Max concurrent tool/action executions |

## Interface

### Configuration

```cpp
void setQuota(const std::string& user_id, const UserQuota& quota);
UserQuota getQuota(const std::string& user_id) const;
```

Creates the user record if absent. `AgentManager::setUserQuota` delegates here.

### Acquisition (non-blocking)

```cpp
bool tryAcquireAgent(const std::string& user_id);
void releaseAgent(const std::string& user_id);

bool tryAcquireLLM(const std::string& user_id);
void releaseLLM(const std::string& user_id);

bool tryAcquireTool(const std::string& user_id);
void releaseTool(const std::string& user_id);
```

`tryAcquire*` returns `true` and increments the counter if below the limit. Returns `false` without blocking if the limit is reached. `release*` decrements the counter.

### Status

```cpp
nlohmann::json usageJson(const std::string& user_id) const;
```

Returns a snapshot of current usage for a user:

```json
{
  "agents_active": 2,
  "llm_inflight": 1,
  "tools_inflight": 5,
  "limits": {
    "max_concurrent_agents": 10,
    "max_llm_inflight": 5,
    "max_tool_inflight": 20
  }
}
```

## `QuotaGuard`

RAII guard that releases a quota slot automatically on destruction.

```cpp
struct QuotaGuard {
    enum class Resource { Agent, LLM, Tool };
    QuotaGuard(QuotaManager& qm, std::string user_id, Resource res);
    ~QuotaGuard();
};
```

Construct a `QuotaGuard` after a successful `tryAcquire*` call to ensure the slot is released even if an exception occurs:

```cpp
if (!qm.tryAcquireAgent(user_id)) throw QuotaExceeded{};
QuotaGuard guard(qm, user_id, QuotaGuard::Resource::Agent);
// ... run agent ...
// guard destructor calls releaseAgent automatically
```

## Thread-Safety

All methods acquire `m_mutex` for the duration of the operation. Concurrent calls from multiple threads are safe.

## C ABI

`am_set_user_quota` serialises `quota_json` and calls `setQuota`. `am_spawn_agent` checks `tryAcquireAgent` before creating the agent entry; returns `AM_ERROR_QUOTA_EXCEEDED` on failure.

## Related Components

- [`AgentManager`](agent_manager.md) — owns the `QuotaManager`; checks quota before spawning agents
- [`C ABI`](c_api.md) — `am_set_user_quota`
