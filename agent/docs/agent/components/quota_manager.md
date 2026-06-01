# QuotaManager

`include/agent/quota.hpp` · `src/agent/quota.cpp`

---

## Overview

`QuotaManager` enforces per-user resource limits. It tracks three independent counters per user — concurrent agents, in-flight LLM calls, and in-flight tool/action calls — and provides non-blocking `tryAcquire` / `release` pairs and a RAII `QuotaGuard` for automatic release.

One `QuotaManager` instance lives inside `AgentManager`.

---

## UserQuota

```cpp
struct UserQuota {
    int max_concurrent_agents{10};  // max agents running at once
    int max_llm_inflight{5};        // max concurrent LLM calls
    int max_tool_inflight{20};      // max concurrent tool/action calls
};
```

Default limits are generous. Tighten per-user with `setUserQuota` or the C ABI `am_set_user_quota`.

---

## API

### `setQuota`

```cpp
void setQuota(const std::string& user_id, const UserQuota& quota);
```

Creates the user record if absent, then sets limits. Safe to call at any time; existing in-flight counts are not affected.

### `getQuota`

```cpp
UserQuota getQuota(const std::string& user_id) const;
```

Returns the current limits. Returns defaults if `user_id` was never configured.

### Acquire / Release

```cpp
bool tryAcquireAgent(const std::string& user_id);
void releaseAgent(const std::string& user_id);

bool tryAcquireLLM(const std::string& user_id);
void releaseLLM(const std::string& user_id);

bool tryAcquireTool(const std::string& user_id);
void releaseTool(const std::string& user_id);
```

`tryAcquire*` returns `true` and atomically increments the counter if the current value is below the limit. Returns `false` without blocking if the limit has been reached.

`release*` decrements the counter. Does not check for underflow — callers must pair each acquire with exactly one release.

### `usageJson`

```cpp
nlohmann::json usageJson(const std::string& user_id) const;
```

Returns the current usage snapshot:
```json
{
  "agents_active":  2,
  "llm_inflight":   1,
  "tools_inflight": 4,
  "quota": {
    "max_concurrent_agents": 10,
    "max_llm_inflight":       5,
    "max_tool_inflight":     20
  }
}
```

---

## QuotaGuard

RAII wrapper that calls `release` in its destructor.

```cpp
struct QuotaGuard {
    enum class Resource { Agent, LLM, Tool };
    QuotaGuard(QuotaManager& qm, std::string user_id, Resource res);
    ~QuotaGuard();  // calls the matching release*
};
```

Usage:

```cpp
if (!quota.tryAcquireAgent(user_id))
    throw std::runtime_error("quota exceeded");
QuotaGuard guard(quota, user_id, QuotaGuard::Resource::Agent);
// ... do work ...
// guard destructor calls releaseAgent(user_id) automatically
```

`AgentManager::spawnAgent` follows this pattern. The guard is released when the agent loop thread finishes.

---

## Where Quotas Are Checked

| Resource | Checked in |
|---|---|
| Agent | `AgentManager::spawnAgent` |
| LLM | Each `Stage::execute` (before the LLM call) |
| Tool | Each `Action::execute` (before the operation) |

A failed `tryAcquire*` in `spawnAgent` throws `std::runtime_error("quota exceeded")`. Via the C ABI this becomes `AM_ERROR_QUOTA_EXCEEDED = 11`.

---

## Thread-Safety

All methods acquire `m_mutex` exclusively. The per-user `UserState` is a plain struct (not atomic) so the mutex guards all reads and writes.

For very high throughput, per-user locks or atomic counters would reduce contention.

---

## C ABI Exposure

```c
am_status_t am_set_user_quota(AgentManager* mgr, const char* user_id,
                               const char* quota_json);
```

`quota_json`:
```json
{"max_concurrent_agents": 5, "max_llm_inflight": 2, "max_tool_inflight": 10}
```

See [C ABI](c_api.md) for the full interface.

---

## Related Components

- [`AgentManager`](agent_manager.md) — checks agent quota on spawn; exposes `setUserQuota`
- [C ABI](c_api.md) — `am_set_user_quota`, `AM_ERROR_QUOTA_EXCEEDED`
- [`EventBus`](event_bus.md) — emits `quota_exceeded` event when a limit is hit
