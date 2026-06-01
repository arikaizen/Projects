# C ABI — libagent_engine

`include/agent_engine/c_api.h` · `src/c_api/c_api.cpp` · `agent_engine.map`

---

## Overview

The C ABI exposes every `AgentManager` capability through a stable C interface. It allows `libagent_engine.so` to be consumed from any language with a C FFI (Python, Go, Rust, GUI toolkits, etc.) without exposing any C++ types across the boundary.

**ABI version:** 1 (returned by `am_api_version()`).

---

## Design Rules

| Rule | Implementation |
|---|---|
| No C++ types cross the ABI | All structured data is null-terminated UTF-8 JSON |
| Opaque handles | `AgentManager*` and `AgentFuture*` are pointers to incomplete types |
| Buffer-out pattern | `(char* buf, size_t out_size)` pairs; return value = bytes needed (excl. null) |
| Thread-local last-error | `am_last_error()` per thread; valid until next ABI call on that thread |
| No exceptions across the ABI | Every entry point catches all C++ exceptions and converts to status codes |
| Memory ownership | Engine allocates and frees its own memory; caller-provided buffers remain caller-owned |
| Thread-safety | Every function is safe to call concurrently on the same `AgentManager*` |

---

## Status Codes

```c
typedef enum {
    AM_OK                        = 0,
    AM_ERROR_INVALID_ARG         = 1,
    AM_ERROR_NOT_FOUND           = 2,
    AM_ERROR_INTERNAL            = 3,
    AM_ERROR_TIMEOUT             = 4,
    AM_ERROR_BUFFER_TOO_SMALL    = 5,
    AM_ERROR_NOT_INITIALIZED     = 6,
    AM_ERROR_DEPRECATED          = 7,
    AM_ERROR_CANCELLED           = 8,
    AM_ERROR_PROMPT_NOT_FOUND    = 9,
    AM_ERROR_PROMPT_SUBSTITUTION = 10,
    AM_ERROR_QUOTA_EXCEEDED      = 11,
    AM_ERROR_DEPENDENCY_CYCLE    = 12,
} am_status_t;
```

---

## Buffer-Out Pattern

Functions that return variable-length JSON use the buffer-out idiom:

```c
char buf[256];
am_status_t s = am_get_status(mgr, agent_id, buf, sizeof(buf));
if (s == AM_ERROR_BUFFER_TOO_SMALL) {
    // retry — the return value tells us how many bytes are needed
    size_t need = ...; // re-call with buf=NULL, out_size=0 to query size
    char* big = malloc(need + 1);
    am_get_status(mgr, agent_id, big, need + 1);
    // use big...
    free(big);
}
```

When `buf == NULL` or `out_size == 0`, the function returns the number of bytes the full output would occupy (excluding the null terminator), allowing a two-pass size-then-fill pattern.

---

## Lifecycle

### `am_create`

```c
AgentManager* am_create(const char* config_json);
```

Creates an `AgentManager`. Returns `NULL` on failure; call `am_last_error(NULL)` for the message.

`config_json` keys (all optional):

| Key | Type | Default | Description |
|---|---|---|---|
| `prompts_dir` | string | `"./prompts"` | Prompt template directory |
| `thread_pool_size` | int | `16` | Worker thread count |
| `max_agent_depth` | int | `3` | Sub-agent nesting limit |
| `default_user_id` | string | `"default"` | Default user id |

### `am_destroy`

```c
void am_destroy(AgentManager* mgr);
```

Cancels all running agents, joins threads, shuts down the pool, frees all memory.

### `am_last_error`

```c
const char* am_last_error(AgentManager* mgr);
```

Returns the thread-local error string from the most recent failed call. Valid until the next ABI call on the current thread. `mgr` may be `NULL` (used when `am_create` itself fails).

### `am_api_version`

```c
int am_api_version(void);
```

Returns `1`.

---

## Agent Lifecycle

### `am_spawn_agent`

```c
am_status_t am_spawn_agent(AgentManager* mgr, const char* config_json,
                            char* out_id_buf, size_t out_size);
```

Spawns a new agent. `out_id_buf` receives the agent id string.

`config_json` keys:

| Key | Type | Default | Description |
|---|---|---|---|
| `user_id` | string | `default_user_id` | Owner; quotas enforced per user |
| `name` | string | `"agent"` | Human-readable name |
| `max_iterations` | int | `100` | Loop iteration cap |
| `max_depth` | int | `3` | Sub-agent depth cap |
| `extra` | object | `{}` | Passed through to `AgentConfig::extra` |

Returns `AM_ERROR_QUOTA_EXCEEDED` if the user is at their agent limit.

### `am_destroy_agent`

```c
am_status_t am_destroy_agent(AgentManager* mgr, const char* agent_id);
```

Cancels and destroys the agent (joins the runner thread).

### `am_list_agents`

```c
am_status_t am_list_agents(AgentManager* mgr, const char* user_id,
                            char* out_json, size_t out_size);
```

JSON array of status objects. Pass `user_id=""` for all agents.

### `am_get_status`

```c
am_status_t am_get_status(AgentManager* mgr, const char* agent_id,
                           char* out_json, size_t out_size);
```

Returns `{"id","name","status","user_id","iterations","result?"}`.

### `am_cancel_agent`

```c
am_status_t am_cancel_agent(AgentManager* mgr, const char* agent_id);
```

Sets the cancellation flag. The agent stops after the current batch.

---

## Pattern A — Run / Future / Pipe

### `am_run_agent`

```c
AgentFuture* am_run_agent(AgentManager* mgr, const char* agent_id,
                           const char* task_json);
```

Starts the agent loop. Returns an `AgentFuture*` (must be freed with `am_future_free`). Returns `NULL` on error.

### `am_future_wait`

```c
am_status_t am_future_wait(AgentFuture* future, int timeout_ms,
                            char* out_result_json, size_t out_size);
```

Blocks until the future resolves or `timeout_ms` elapses. Pass `-1` to wait forever, `0` to poll.

Returns `AM_ERROR_TIMEOUT` if the deadline passes.

### `am_future_free`

```c
void am_future_free(AgentFuture* future);
```

Frees the future object without cancelling the agent.

### `am_pipe`

```c
am_status_t am_pipe(AgentManager* mgr, const char* from_id,
                     const char* to_id, const char* template_string);
```

Registers a delegation pipe. When `from_id` finishes, `template_string` is applied to its output (`{prev_output}` substitution) and given as the task to `to_id`.

---

## Pattern B — Messaging

```c
am_status_t am_send_message(AgentManager* mgr, const char* from,
                             const char* to, const char* msg_json);
am_status_t am_broadcast(AgentManager* mgr, const char* from,
                          const char* msg_json);
am_status_t am_drain_inbox(AgentManager* mgr, const char* agent_id,
                            char* out_json, size_t out_size);
```

`am_drain_inbox` returns a JSON array of `Message` objects.

---

## Pattern C — Blackboard

```c
am_status_t am_blackboard_write(AgentManager* mgr, const char* key,
                                 const char* value_json);
am_status_t am_blackboard_read(AgentManager* mgr, const char* key,
                                char* out_value_json, size_t out_size);
am_status_t am_blackboard_keys(AgentManager* mgr, const char* prefix,
                                char* out_json, size_t out_size);
```

---

## Fan-Out / Fan-In

### `am_fan_out`

```c
am_status_t am_fan_out(AgentManager* mgr, const char* configs_json_array,
                        const char* shared_task,
                        AgentFuture*** out_futures, size_t* out_count);
```

Spawns N agents and starts them all with `shared_task`. Allocates an array of `AgentFuture*`; caller frees each future with `am_future_free` and the array with `am_fan_out_free_array`.

```c
void am_fan_out_free_array(AgentFuture** arr);
```

### `am_research_from_angles`

```c
am_status_t am_research_from_angles(AgentManager* mgr,
                                     const char* angles_json_array,
                                     const char* topic,
                                     char* out_result_json, size_t out_size);
```

Convenience fan-out + fan-in: one researcher per angle, one synthesiser. Blocks until the synthesiser finishes.

---

## Real-Time Injection

```c
am_status_t am_inject_work(AgentManager* mgr, const char* agent_id,
                            const char* work_item_json);
```

`work_item_json`:
```json
{
  "name":     "ReadAction",
  "id":       "r42",
  "inputs":   {"path": "/tmp/file.txt"},
  "position": "front"
}
```

---

## Events

```c
typedef void (*am_event_cb)(const char* event_json, void* user_data);

am_status_t am_subscribe_events(AgentManager* mgr, am_event_cb cb, void* user_data);
am_status_t am_unsubscribe_events(AgentManager* mgr, am_event_cb cb);
```

Callbacks fire on engine threads. GUI consumers must marshal to their UI thread before touching UI objects.

See [`EventBus`](event_bus.md) for the full list of event types and their fields.

---

## Hot Reload / Config

```c
am_status_t am_reload_prompts(AgentManager* mgr);
am_status_t am_set_prompts_dir(AgentManager* mgr, const char* dir_path);
am_status_t am_set_user_quota(AgentManager* mgr, const char* user_id,
                               const char* quota_json);
```

`quota_json`:
```json
{"max_concurrent_agents": 5, "max_llm_inflight": 2, "max_tool_inflight": 10}
```

---

## MCP Management

```c
am_status_t am_connect_mcp(AgentManager* mgr, const char* server_config_json);
am_status_t am_disconnect_mcp(AgentManager* mgr, const char* server_name);
am_status_t am_list_mcp_servers(AgentManager* mgr, char* out_json, size_t out_size);
```

`server_config_json`:
```json
{"name": "myserver", "url": "http://localhost:8080", "extra": {}}
```

---

## Symbol Visibility

`agent_engine.map` (a GNU ld version script) restricts exported symbols:

```
AGENT_ENGINE_1 {
  global: am_api_version; am_create; am_destroy; ... (all am_* symbols)
  local: *;
};
```

Only the `am_*` functions are visible to consumers. All C++ symbols from `agent_core` are hidden, preventing ABI pollution.

---

## Internal Implementation Notes

- Each C function follows the pattern: `try { ... return AM_OK; } catch (std::exception& e) { set_last_error(e.what()); return AM_ERROR_INTERNAL; }`.
- Handles are `reinterpret_cast<AgentManager*>` of the actual C++ object.
- `AgentFuture` wraps a `std::future<nlohmann::json>` and the agent id; allocated with `new`, freed by `am_future_free`.
- The thread-local last-error string is a `thread_local std::string` in `c_api.cpp`.

---

## Related Components

- [`AgentManager`](agent_manager.md) — the C++ object behind every handle
- All other components — exposed through this interface
- [`examples/cli_driver.cpp`](../../examples/cli_driver.cpp) — end-to-end C ABI usage demo
