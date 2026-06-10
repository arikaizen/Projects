# C ABI — libagent_engine

`include/agent_engine/c_api.h` · `src/c_api/c_api.cpp`

## Overview

`libagent_engine` exposes the full C++ agent engine through a stable C ABI. No C++ types cross the boundary — all structured data is null-terminated UTF-8 JSON. The ABI is versioned (`am_api_version()` returns `1`).

## Design Rules

| Rule | Detail |
|---|---|
| No C++ types | All structured data is JSON strings |
| Opaque handles | `AgentManager*` and `AgentFuture*` are pointers to incomplete struct types |
| Buffer-out pattern | Caller provides `(buf, size)`; return value is bytes that would have been written (excluding null). If `ret >= out_size`, retry with a larger buffer |
| Thread-local last-error | `am_last_error(mgr)` returns the most recent error for the calling thread; valid until the next ABI call on that thread |
| No exceptions | Every entry point catches all C++ exceptions and converts to status codes |
| Memory ownership | Engine allocates and frees its own memory; never return a `char*` the caller must `free()` |
| Thread-safety | Every function is safe to call concurrently on the same `AgentManager*` |

## Status Codes

```c
typedef enum {
    AM_OK                       = 0,
    AM_ERROR_INVALID_ARG        = 1,
    AM_ERROR_NOT_FOUND          = 2,
    AM_ERROR_INTERNAL           = 3,
    AM_ERROR_TIMEOUT            = 4,
    AM_ERROR_BUFFER_TOO_SMALL   = 5,
    AM_ERROR_NOT_INITIALIZED    = 6,
    AM_ERROR_DEPRECATED         = 7,
    AM_ERROR_CANCELLED          = 8,
    AM_ERROR_PROMPT_NOT_FOUND   = 9,
    AM_ERROR_PROMPT_SUBSTITUTION = 10,
    AM_ERROR_QUOTA_EXCEEDED     = 11,
    AM_ERROR_DEPENDENCY_CYCLE   = 12,
} am_status_t;
```

## Lifecycle

### `am_create`

```c
AgentManager* am_create(const char* config_json);
```

Creates an `AgentManager` from a JSON config object. Returns `NULL` on failure; call `am_last_error(NULL)` for the message.

**Config keys:**

| Key | Type | Default | Meaning |
|---|---|---|---|
| `prompts_dir` | string | `"./prompts"` | Prompt template directory |
| `thread_pool_size` | int | `16` | Worker thread count |
| `max_agent_depth` | int | `3` | Sub-agent nesting limit |
| `default_user_id` | string | `"default"` | Default user id |
| `llm` | object | — | LLM backend config (see below) |

**LLM config (`config["llm"]`):**

| Backend | Key | Required fields |
|---|---|---|
| Mock (default) | `"backend": "mock"` | None |
| vLLM | `"backend": "vllm"` | `"base_url"`, `"model_name"` (requires `-DAGENT_ENABLE_VLLM=ON`) |

### `am_destroy`

```c
void am_destroy(AgentManager* mgr);
```

Cancels all running agents, joins their threads, and frees all resources.

### `am_last_error` / `am_api_version`

```c
const char* am_last_error(AgentManager* mgr);
int         am_api_version(void);
```

## Agent Lifecycle

```c
am_status_t am_spawn_agent(AgentManager* mgr, const char* config_json,
                            char* out_id_buf, size_t out_size);
am_status_t am_destroy_agent(AgentManager* mgr, const char* agent_id);
am_status_t am_list_agents(AgentManager* mgr, const char* user_id,
                            char* out_json, size_t out_size);
am_status_t am_get_status(AgentManager* mgr, const char* agent_id,
                           char* out_json, size_t out_size);
am_status_t am_cancel_agent(AgentManager* mgr, const char* agent_id);
```

`am_spawn_agent` config keys: `user_id`, `name`, `max_iterations`, `max_depth`, `extra`.

Returns `AM_ERROR_QUOTA_EXCEEDED` if the user is at their agent limit.

## Pattern A — Run / Future / Pipe

```c
AgentFuture* am_run_agent(AgentManager* mgr, const char* agent_id,
                           const char* task_json);
am_status_t  am_future_wait(AgentFuture* future, int timeout_ms,
                             char* out_result_json, size_t out_size);
void         am_future_free(AgentFuture* future);
am_status_t  am_pipe(AgentManager* mgr, const char* from_id,
                      const char* to_id, const char* template_string);
```

`am_future_wait`: `timeout_ms = -1` means wait forever; `0` is a poll.

## Pattern B — Messaging

```c
am_status_t am_send_message(AgentManager* mgr, const char* from,
                             const char* to, const char* msg_json);
am_status_t am_broadcast(AgentManager* mgr, const char* from,
                          const char* msg_json);
am_status_t am_drain_inbox(AgentManager* mgr, const char* agent_id,
                            char* out_json, size_t out_size);
```

## Pattern C — Blackboard

```c
am_status_t am_blackboard_write(AgentManager* mgr, const char* key,
                                 const char* value_json);
am_status_t am_blackboard_read(AgentManager* mgr, const char* key,
                                char* out_value_json, size_t out_size);
am_status_t am_blackboard_keys(AgentManager* mgr, const char* prefix,
                                char* out_json, size_t out_size);
```

## Fan-Out / Fan-In

```c
am_status_t am_fan_out(AgentManager* mgr, const char* configs_json_array,
                        const char* shared_task,
                        AgentFuture*** out_futures, size_t* out_count);
void        am_fan_out_free_array(AgentFuture** arr);
am_status_t am_research_from_angles(AgentManager* mgr,
                                     const char* angles_json_array,
                                     const char* topic,
                                     char* out_result_json, size_t out_size);
```

`am_fan_out` allocates an array of `AgentFuture*`. The caller frees each future with `am_future_free` then frees the array with `am_fan_out_free_array`.

## Real-Time Injection

```c
am_status_t am_inject_work(AgentManager* mgr, const char* agent_id,
                            const char* work_item_json);
```

`work_item_json` format: `{"name":"ReadAction","id":"r42","inputs":{...},"position":"front"|"back"}`.

## Events

```c
typedef void (*am_event_cb)(const char* event_json, void* user_data);

am_status_t am_subscribe_events(AgentManager* mgr, am_event_cb cb, void* user_data);
am_status_t am_unsubscribe_events(AgentManager* mgr, am_event_cb cb);
```

Callbacks fire on engine threads. See [EventBus](event_bus.md) for the full list of event types.

## Hot Reload / Config

```c
am_status_t am_reload_prompts(AgentManager* mgr);
am_status_t am_set_prompts_dir(AgentManager* mgr, const char* dir_path);
am_status_t am_set_user_quota(AgentManager* mgr, const char* user_id,
                               const char* quota_json);
```

`quota_json`: `{"max_concurrent_agents":N, "max_llm_inflight":N, "max_tool_inflight":N}`.

## LLM Management

```c
am_status_t am_configure_llm(AgentManager* mgr, const char* llm_config_json);
```

Swaps the default LLM backend at runtime. `llm_config_json` selects a provider
(OpenAI/ChatGPT, Anthropic/Claude, Google Gemini, Ollama, Groq, Mistral,
DeepSeek, Grok, OpenRouter, Together, LM Studio, llama.cpp, vLLM, in-process
`llama`, `custom`, or `mock`). A per-agent override may be passed as the `llm`
key in `am_spawn_agent`. See [LLM_PROVIDERS.md](../../LLM_PROVIDERS.md) for the
config shape and build flags.

## MCP Management

```c
am_status_t am_connect_mcp(AgentManager* mgr, const char* server_config_json);
am_status_t am_disconnect_mcp(AgentManager* mgr, const char* server_name);
am_status_t am_list_mcp_servers(AgentManager* mgr, char* out_json, size_t out_size);
```

## Related Components

- [`AgentManager`](agent_manager.md) — all `am_*` functions delegate here
- [`EventBus`](event_bus.md) — event types emitted to `am_event_cb` subscribers
- [`QuotaManager`](quota_manager.md) — `am_set_user_quota`, `AM_ERROR_QUOTA_EXCEEDED`
- [`PromptLoader`](prompt_loader.md) — `am_reload_prompts`, `am_set_prompts_dir`
