/**
 * agent_engine/c_api.h — Public C ABI for libagent_engine.
 *
 * Design rules:
 *   - No C++ types cross the ABI.  All structured data is null-terminated
 *     UTF-8 JSON.
 *   - Opaque handles: AgentManager* and AgentFuture* are pointers to
 *     incomplete struct types.
 *   - Buffer-out pattern: the caller provides a (buf, size) pair; the function
 *     writes UTF-8 JSON and returns the number of bytes that would have been
 *     written (excluding the null terminator).  If the return value is >=
 *     out_size the buffer was too small; retry with a larger buffer.
 *   - Thread-local last-error: am_last_error() returns the most recent error
 *     string for the calling thread.  Valid until the next ABI call on that
 *     thread.
 *   - No exceptions cross the ABI.  Every entry point catches all C++
 *     exceptions and converts them to status codes.
 *   - Memory ownership: the engine allocates and frees its own memory.
 *     Caller-provided buffers remain caller-owned.  Never return a char* the
 *     caller must free().
 *   - Thread-safety: every function is safe to call concurrently from multiple
 *     threads on the same AgentManager*.  Event callbacks fire on engine
 *     threads; GUI consumers must marshal to their UI thread.
 *
 * ABI version history:
 *   1 — initial release (requirements 1–60, worked examples 1–14)
 */

#ifndef AGENT_ENGINE_C_API_H
#define AGENT_ENGINE_C_API_H

#include <stddef.h>  /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Opaque handles ─────────────────────────────────────────────────────── */

typedef struct AgentManager_ AgentManager;
typedef struct AgentFuture_  AgentFuture;

/* ── Status codes ───────────────────────────────────────────────────────── */

typedef enum {
    AM_OK                      = 0,
    AM_ERROR_INVALID_ARG       = 1,
    AM_ERROR_NOT_FOUND         = 2,
    AM_ERROR_INTERNAL          = 3,
    AM_ERROR_TIMEOUT           = 4,
    AM_ERROR_BUFFER_TOO_SMALL  = 5,
    AM_ERROR_NOT_INITIALIZED   = 6,
    AM_ERROR_DEPRECATED        = 7,
    AM_ERROR_CANCELLED         = 8,
    AM_ERROR_PROMPT_NOT_FOUND  = 9,
    AM_ERROR_PROMPT_SUBSTITUTION = 10,
    AM_ERROR_QUOTA_EXCEEDED    = 11,
    AM_ERROR_DEPENDENCY_CYCLE  = 12,
} am_status_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * am_create — create an AgentManager from a JSON config object.
 *
 * config_json keys (all optional):
 *   "prompts_dir"       : string  — path to prompt template directory
 *   "thread_pool_size"  : int     — number of worker threads (default 16)
 *   "max_agent_depth"   : int     — max sub-agent nesting depth (default 3)
 *   "default_user_id"   : string  — user id used when none is supplied
 *   "llm"               : object  — LLM connection config (backend-specific)
 *
 * Returns NULL on failure; call am_last_error(NULL) for the message.
 */
AgentManager* am_create(const char* config_json);

/** Shut down the manager and free all resources.  Cancels running agents. */
void am_destroy(AgentManager* mgr);

/**
 * Thread-local last error string.  Valid until the next ABI call on this
 * thread.  mgr may be NULL (used when am_create itself fails).
 */
const char* am_last_error(AgentManager* mgr);

/** Returns the ABI version integer (currently 1). */
int am_api_version(void);

/* ── Agent lifecycle ────────────────────────────────────────────────────── */

/**
 * am_spawn_agent — create a new agent from a JSON config object.
 *
 * config_json keys:
 *   "user_id"        : string  — owner; quotas are enforced per user
 *   "name"           : string  — human-readable name
 *   "max_iterations" : int     — loop iteration cap (default 100)
 *   "max_depth"      : int     — sub-agent depth cap (default 3)
 *   "extra"          : object  — passed to the agent unchanged
 *
 * out_id_buf receives the new agent id (UTF-8, null-terminated).
 * Returns AM_ERROR_QUOTA_EXCEEDED if the user is at their agent limit.
 */
am_status_t am_spawn_agent(AgentManager* mgr, const char* config_json,
                            char* out_id_buf, size_t out_size);

/** Cancel and destroy an agent.  Safe to call on a running agent. */
am_status_t am_destroy_agent(AgentManager* mgr, const char* agent_id);

/**
 * am_list_agents — JSON array of agent status objects for user_id.
 * Pass user_id="" to list all agents (admin view).
 */
am_status_t am_list_agents(AgentManager* mgr, const char* user_id,
                            char* out_json, size_t out_size);

/** Status object: {"id","name","status","user_id","iterations","result?"}. */
am_status_t am_get_status(AgentManager* mgr, const char* agent_id,
                           char* out_json, size_t out_size);

/** Set the cancellation flag.  The agent stops after the current batch. */
am_status_t am_cancel_agent(AgentManager* mgr, const char* agent_id);

/* ── Pattern A — run / future / pipe ────────────────────────────────────── */

/**
 * am_run_agent — start the agent on the thread pool with the given task.
 *
 * task_json: {"task": "..."} or bare string.
 * Returns an AgentFuture* that must be freed with am_future_free().
 * Returns NULL on error (check am_last_error).
 */
AgentFuture* am_run_agent(AgentManager* mgr, const char* agent_id,
                           const char* task_json);

/**
 * am_future_wait — block until the future completes or timeout_ms elapses.
 *
 * timeout_ms: -1 = wait forever, 0 = poll (no wait).
 * out_result_json receives the agent's final output JSON.
 * Returns AM_ERROR_TIMEOUT if the deadline passes before completion.
 */
am_status_t am_future_wait(AgentFuture* future, int timeout_ms,
                            char* out_result_json, size_t out_size);

/** Free the future object.  Does not cancel the agent. */
void am_future_free(AgentFuture* future);

/**
 * am_pipe — register an output-to-input pipe between two agents.
 *
 * When from_id finishes, template_string is applied to its output
 * (substituting {prev_output} and other placeholders) and the result is
 * given as the task to to_id.  Simple string substitution only in v1.
 */
am_status_t am_pipe(AgentManager* mgr, const char* from_id,
                     const char* to_id, const char* template_string);

/* ── Pattern B — messaging ───────────────────────────────────────────────── */

/** Send msg_json to to's inbox from from's identity. */
am_status_t am_send_message(AgentManager* mgr, const char* from,
                             const char* to, const char* msg_json);

/** Broadcast msg_json to every other agent's inbox. */
am_status_t am_broadcast(AgentManager* mgr, const char* from,
                          const char* msg_json);

/** Drain and return agent_id's inbox as a JSON array. */
am_status_t am_drain_inbox(AgentManager* mgr, const char* agent_id,
                            char* out_json, size_t out_size);

/* ── Pattern C — blackboard ─────────────────────────────────────────────── */

/** Write a JSON value under key. */
am_status_t am_blackboard_write(AgentManager* mgr, const char* key,
                                 const char* value_json);

/** Read the value under key into out_value_json. */
am_status_t am_blackboard_read(AgentManager* mgr, const char* key,
                                char* out_value_json, size_t out_size);

/** Return JSON array of keys with the given prefix. */
am_status_t am_blackboard_keys(AgentManager* mgr, const char* prefix,
                                char* out_json, size_t out_size);

/* ── Composition — fan-out / fan-in ─────────────────────────────────────── */

/**
 * am_fan_out — spawn N agents from configs_json_array (JSON array of config
 * objects) and start them all with shared_task.  Returns an array of
 * AgentFuture* in *out_futures (length *out_count).  Caller must free each
 * future with am_future_free() and free the array itself with
 * am_fan_out_free_array().
 */
am_status_t am_fan_out(AgentManager* mgr, const char* configs_json_array,
                        const char* shared_task,
                        AgentFuture*** out_futures, size_t* out_count);

/** Free the array returned by am_fan_out (not the futures themselves). */
void am_fan_out_free_array(AgentFuture** arr);

/**
 * am_research_from_angles — convenience: fan-out + fan-in.
 * angles_json_array: JSON array of angle strings, e.g. ["legal","technical"].
 * Each worker writes findings to the blackboard; a synthesiser combines them.
 * Blocks until the synthesiser finishes.
 */
am_status_t am_research_from_angles(AgentManager* mgr,
                                     const char* angles_json_array,
                                     const char* topic,
                                     char* out_result_json, size_t out_size);

/* ── Real-time injection ────────────────────────────────────────────────── */

/**
 * am_inject_work — inject a work item into a running agent's queue.
 *
 * work_item_json: {"name":"ReadAction","id":"r42","inputs":{...},"position":"front"|"back"}
 * The item is constructed via WorkFactory and pushed to the queue.
 * Takes effect at the next batch boundary (the current batch is not interrupted).
 */
am_status_t am_inject_work(AgentManager* mgr, const char* agent_id,
                            const char* work_item_json);

/* ── Events ─────────────────────────────────────────────────────────────── */

/**
 * Event callback type.  event_json is a UTF-8 JSON object with at minimum:
 *   {"type":"...", "timestamp":"ISO8601", "agent_id":"..." (if applicable),
 *    "user_id":"..." (if applicable)}
 *
 * WARNING: callbacks fire on engine (thread-pool) threads.  GUI consumers
 * MUST marshal to their UI thread before touching UI objects.
 *
 * Event types emitted:
 *   agent_spawned, agent_started, agent_finished, agent_failed,
 *   agent_cancelled, agent_destroyed,
 *   work_item_started (includes ran_in_parallel:bool),
 *   work_item_finished (includes success, duration_ms, ran_in_parallel),
 *   batch_started, batch_finished,
 *   work_injected,
 *   message_sent, message_received,
 *   blackboard_updated,
 *   mcp_connected, mcp_disconnected, mcp_notification,
 *   prompts_reloaded,
 *   quota_exceeded.
 */
typedef void (*am_event_cb)(const char* event_json, void* user_data);

/** Subscribe to all events.  cb+user_data is the unique key for unsubscription. */
am_status_t am_subscribe_events(AgentManager* mgr, am_event_cb cb,
                                 void* user_data);

/** Unsubscribe by the same cb pointer used in am_subscribe_events. */
am_status_t am_unsubscribe_events(AgentManager* mgr, am_event_cb cb);

/* ── Hot reload / config ────────────────────────────────────────────────── */

/** Drop the PromptLoader cache; templates are re-read on next use. */
am_status_t am_reload_prompts(AgentManager* mgr);

/** Change the prompts directory; implies am_reload_prompts. */
am_status_t am_set_prompts_dir(AgentManager* mgr, const char* dir_path);

/**
 * am_set_user_quota — configure per-user limits.
 * quota_json: {"max_concurrent_agents":N, "max_llm_inflight":N,
 *              "max_tool_inflight":N}
 */
am_status_t am_set_user_quota(AgentManager* mgr, const char* user_id,
                               const char* quota_json);

/* ── MCP management ─────────────────────────────────────────────────────── */

/**
 * am_connect_mcp — connect to an MCP server and register its tools.
 * server_config_json: {"name":"myserver","url":"http://...","extra":{}}
 */
am_status_t am_connect_mcp(AgentManager* mgr, const char* server_config_json);

/** Disconnect and deregister all tools for server_name. */
am_status_t am_disconnect_mcp(AgentManager* mgr, const char* server_name);

/** JSON array of connected server names. */
am_status_t am_list_mcp_servers(AgentManager* mgr,
                                 char* out_json, size_t out_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AGENT_ENGINE_C_API_H */
