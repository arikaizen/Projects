/**
 * src/c_api/c_api.cpp — C ABI wrapper over agent::AgentManager.
 *
 * Design notes:
 *   - Every public symbol is wrapped in extern "C" to suppress C++ name
 *     mangling.  The C header already declares the functions; we just provide
 *     the bodies.
 *   - The opaque AgentManager* IS a reinterpret_cast of agent::AgentManager*.
 *     The C side never dereferences the pointer, so no UB occurs.
 *   - AgentFuture_ is a private POD that wraps std::future<nlohmann::json>
 *     plus a cached result string.  It is heap-allocated here and freed via
 *     am_future_free().
 *   - Every entry point catches all C++ exceptions and converts them to status
 *     codes, storing the message in a thread-local string accessible via
 *     am_last_error().
 *   - No C++ types cross the ABI boundary.  All structured data travels as
 *     null-terminated UTF-8 JSON.
 */

#include "agent_engine/c_api.h"
#include "agent/agent_manager.hpp"
#include "agent/agent_context.hpp"
#include "agent/work_factory.hpp"
#include "agent/llm_client.hpp"
#include "agent/memory_backend.hpp"
#include "agent/quota.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <vector>

// ── Thread-local error storage ────────────────────────────────────────────────

thread_local std::string tl_last_error;

static void setError(const std::string& msg) {
    tl_last_error = msg;
}

// ── AgentFuture_ definition ───────────────────────────────────────────────────

// This struct is the "real" type behind the opaque AgentFuture* handle.
// It lives entirely on the C++ side; the C caller only ever holds the pointer.
struct AgentFuture_ {
    std::future<nlohmann::json> future;
    std::string                 result_str; // cached serialization after wait
};

// ── Internal helpers ──────────────────────────────────────────────────────────

// Cast the opaque C handle to our C++ class.
static inline agent::AgentManager* toMgr(AgentManager* mgr) {
    return reinterpret_cast<agent::AgentManager*>(mgr);
}

// Write a JSON string into a caller-provided buffer using the buffer-out
// pattern described in c_api.h:
//   - If buf is non-null and size > strlen(json_str), copy and return AM_OK.
//   - Otherwise zero-terminate buf (if possible) and return AM_ERROR_BUFFER_TOO_SMALL.
static am_status_t writeJson(const std::string& json_str,
                              char*              buf,
                              size_t             size)
{
    size_t needed = json_str.size(); // bytes, excluding null terminator
    if (buf && size > needed) {
        std::memcpy(buf, json_str.c_str(), needed + 1);
        return AM_OK;
    }
    if (buf && size > 0) {
        buf[0] = '\0';
    }
    setError("Buffer too small: need " + std::to_string(needed + 1) + " bytes");
    return AM_ERROR_BUFFER_TOO_SMALL;
}

// Parse a JSON string; on failure set the error and return false.
static bool parseJson(const char* raw, nlohmann::json& out, const char* context) {
    if (!raw) {
        setError(std::string(context) + ": null JSON pointer");
        return false;
    }
    try {
        out = nlohmann::json::parse(raw);
        return true;
    } catch (const std::exception& e) {
        setError(std::string(context) + ": JSON parse error: " + e.what());
        return false;
    }
}

// Validate a required non-null pointer argument.
static bool requireMgr(AgentManager* mgr) {
    if (!mgr) {
        setError("AgentManager pointer is null");
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" {

/**
 * am_create — build an AgentManager from a JSON config object.
 *
 * Accepted JSON keys (all optional):
 *   "prompts_dir"       string
 *   "thread_pool_size"  int
 *   "max_agent_depth"   int
 *   "default_user_id"  string
 *   "llm"              object  (ignored in this build; MockLLMClient is used)
 */
AgentManager* am_create(const char* config_json) {
    try {
        agent::AgentManager::Config cfg;

        // Parse config if provided; silently use defaults on null/empty input.
        if (config_json && config_json[0] != '\0') {
            nlohmann::json j;
            try {
                j = nlohmann::json::parse(config_json);
            } catch (const std::exception& e) {
                setError(std::string("am_create: JSON parse error: ") + e.what());
                return nullptr;
            }

            if (j.contains("prompts_dir") && j["prompts_dir"].is_string()) {
                cfg.prompts_dir = j["prompts_dir"].get<std::string>();
            }
            if (j.contains("thread_pool_size") && j["thread_pool_size"].is_number_integer()) {
                cfg.thread_pool_size = j["thread_pool_size"].get<int>();
            }
            if (j.contains("max_agent_depth") && j["max_agent_depth"].is_number_integer()) {
                cfg.max_agent_depth = j["max_agent_depth"].get<int>();
            }
            if (j.contains("default_user_id") && j["default_user_id"].is_string()) {
                cfg.default_user_id = j["default_user_id"].get<std::string>();
            }
        }

        // MockLLMClient that returns an empty JSON array plan.
        // Real deployments replace this with an HTTP-backed LLMClient.
        auto llm = std::make_shared<agent::MockLLMClient>(
            [](const agent::LLMClient::Request&) -> agent::LLMClient::Response {
                return {
                    /*content=*/ "[]",
                    /*success=*/ true,
                    /*error=*/   ""
                };
            }
        );

        auto memory = std::make_shared<agent::NoOpMemoryBackend>();

        auto* mgr = new agent::AgentManager(cfg, llm, memory);
        return reinterpret_cast<AgentManager*>(mgr);

    } catch (const std::exception& e) {
        setError(std::string("am_create: ") + e.what());
        return nullptr;
    } catch (...) {
        setError("am_create: unknown exception");
        return nullptr;
    }
}

void am_destroy(AgentManager* mgr) {
    if (!mgr) return;
    try {
        delete toMgr(mgr);
    } catch (...) {
        // Destructors should not throw; swallow silently.
    }
}

const char* am_last_error(AgentManager* /*mgr*/) {
    return tl_last_error.c_str();
}

int am_api_version(void) {
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Agent lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_spawn_agent(AgentManager* mgr, const char* config_json,
                            char* out_id_buf, size_t out_size)
{
    if (!requireMgr(mgr))       return AM_ERROR_INVALID_ARG;
    if (!config_json)           { setError("am_spawn_agent: config_json is null"); return AM_ERROR_INVALID_ARG; }
    if (!out_id_buf || !out_size) { setError("am_spawn_agent: output buffer is null/zero"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(config_json, j, "am_spawn_agent")) return AM_ERROR_INVALID_ARG;

        agent::AgentConfig cfg;
        if (j.contains("user_id")        && j["user_id"].is_string())         cfg.agent_id = "";         // auto-assigned
        if (j.contains("name")           && j["name"].is_string())            cfg.name           = j["name"].get<std::string>();
        if (j.contains("task")           && j["task"].is_string())            cfg.task           = j["task"].get<std::string>();
        if (j.contains("max_iterations") && j["max_iterations"].is_number_integer()) cfg.max_iterations = j["max_iterations"].get<int>();
        if (j.contains("max_depth")      && j["max_depth"].is_number_integer())      cfg.max_depth      = j["max_depth"].get<int>();
        if (j.contains("extra"))                                               cfg.extra          = j["extra"];

        // AgentConfig has no dedicated user_id field; AgentManager::spawnAgent
        // reads the owning user from cfg.extra["user_id"] for quota enforcement.
        // Stash it there under the exact key the manager expects.
        if (j.contains("user_id") && j["user_id"].is_string()) {
            if (!cfg.extra.is_object()) cfg.extra = nlohmann::json::object();
            cfg.extra["user_id"] = j["user_id"].get<std::string>();
        }

        std::string agent_id = toMgr(mgr)->spawnAgent(cfg);
        return writeJson(agent_id, out_id_buf, out_size);

    } catch (const std::exception& e) {
        setError(std::string("am_spawn_agent: ") + e.what());
        // Distinguish quota errors by message content (AgentManager throws with
        // "quota exceeded" in the message when the limit is hit).
        std::string msg = e.what();
        if (msg.find("quota") != std::string::npos ||
            msg.find("Quota") != std::string::npos) {
            return AM_ERROR_QUOTA_EXCEEDED;
        }
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_spawn_agent: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_destroy_agent(AgentManager* mgr, const char* agent_id) {
    if (!requireMgr(mgr))  return AM_ERROR_INVALID_ARG;
    if (!agent_id)         { setError("am_destroy_agent: agent_id is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->destroyAgent(agent_id);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_destroy_agent: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_destroy_agent: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_list_agents(AgentManager* mgr, const char* user_id,
                            char* out_json, size_t out_size)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;

    try {
        std::string uid = (user_id ? user_id : "");
        nlohmann::json result = toMgr(mgr)->listAgents(uid);
        return writeJson(result.dump(), out_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_list_agents: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_list_agents: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_get_status(AgentManager* mgr, const char* agent_id,
                           char* out_json, size_t out_size)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!agent_id)        { setError("am_get_status: agent_id is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json status = toMgr(mgr)->getStatus(agent_id);
        return writeJson(status.dump(), out_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_get_status: ") + e.what());
        std::string msg = e.what();
        if (msg.find("not found") != std::string::npos ||
            msg.find("No agent")  != std::string::npos) {
            return AM_ERROR_NOT_FOUND;
        }
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_get_status: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_cancel_agent(AgentManager* mgr, const char* agent_id) {
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!agent_id)        { setError("am_cancel_agent: agent_id is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->cancelAgent(agent_id);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_cancel_agent: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_cancel_agent: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pattern A — run / future / pipe
// ═══════════════════════════════════════════════════════════════════════════════

AgentFuture* am_run_agent(AgentManager* mgr, const char* agent_id,
                           const char* task_json)
{
    if (!requireMgr(mgr)) return nullptr;
    if (!agent_id)        { setError("am_run_agent: agent_id is null"); return nullptr; }
    if (!task_json)       { setError("am_run_agent: task_json is null"); return nullptr; }

    try {
        // Accept either {"task": "..."} or a bare string literal.
        std::string task;
        try {
            nlohmann::json j = nlohmann::json::parse(task_json);
            if (j.is_string()) {
                task = j.get<std::string>();
            } else if (j.is_object() && j.contains("task") && j["task"].is_string()) {
                task = j["task"].get<std::string>();
            } else {
                // Dump whatever came in as the task string.
                task = j.dump();
            }
        } catch (...) {
            // Not valid JSON — treat the raw string as the task.
            task = task_json;
        }

        std::future<nlohmann::json> fut = toMgr(mgr)->runAgent(agent_id, task);
        auto* af = new AgentFuture_{std::move(fut), ""};
        return reinterpret_cast<AgentFuture*>(af);

    } catch (const std::exception& e) {
        setError(std::string("am_run_agent: ") + e.what());
        return nullptr;
    } catch (...) {
        setError("am_run_agent: unknown exception");
        return nullptr;
    }
}

am_status_t am_future_wait(AgentFuture* future, int timeout_ms,
                            char* out_result_json, size_t out_size)
{
    if (!future) { setError("am_future_wait: future is null"); return AM_ERROR_INVALID_ARG; }

    try {
        auto* af = reinterpret_cast<AgentFuture_*>(future);

        // If we already have a cached result, skip the wait.
        if (af->result_str.empty()) {
            if (timeout_ms < 0) {
                // Wait forever.
                nlohmann::json result = af->future.get();
                af->result_str = result.dump();
            } else {
                // Timed wait.
                auto status = af->future.wait_for(
                    std::chrono::milliseconds(timeout_ms));

                if (status == std::future_status::timeout) {
                    setError("am_future_wait: timeout after " +
                             std::to_string(timeout_ms) + " ms");
                    return AM_ERROR_TIMEOUT;
                }
                nlohmann::json result = af->future.get();
                af->result_str = result.dump();
            }
        }

        return writeJson(af->result_str, out_result_json, out_size);

    } catch (const std::exception& e) {
        setError(std::string("am_future_wait: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_future_wait: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

void am_future_free(AgentFuture* future) {
    if (!future) return;
    try {
        delete reinterpret_cast<AgentFuture_*>(future);
    } catch (...) {}
}

am_status_t am_pipe(AgentManager* mgr, const char* from_id,
                     const char* to_id, const char* template_string)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!from_id)         { setError("am_pipe: from_id is null"); return AM_ERROR_INVALID_ARG; }
    if (!to_id)           { setError("am_pipe: to_id is null");   return AM_ERROR_INVALID_ARG; }
    if (!template_string) { setError("am_pipe: template_string is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->pipe(from_id, to_id, template_string);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_pipe: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_pipe: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pattern B — messaging
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_send_message(AgentManager* mgr, const char* from,
                             const char* to, const char* msg_json)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!from)            { setError("am_send_message: from is null"); return AM_ERROR_INVALID_ARG; }
    if (!to)              { setError("am_send_message: to is null");   return AM_ERROR_INVALID_ARG; }
    if (!msg_json)        { setError("am_send_message: msg_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json payload;
        if (!parseJson(msg_json, payload, "am_send_message")) return AM_ERROR_INVALID_ARG;

        toMgr(mgr)->sendMessage(from, to, payload);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_send_message: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_send_message: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_broadcast(AgentManager* mgr, const char* from,
                          const char* msg_json)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!from)            { setError("am_broadcast: from is null"); return AM_ERROR_INVALID_ARG; }
    if (!msg_json)        { setError("am_broadcast: msg_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json payload;
        if (!parseJson(msg_json, payload, "am_broadcast")) return AM_ERROR_INVALID_ARG;

        toMgr(mgr)->broadcast(from, payload);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_broadcast: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_broadcast: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_drain_inbox(AgentManager* mgr, const char* agent_id,
                            char* out_json, size_t out_size)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!agent_id)        { setError("am_drain_inbox: agent_id is null"); return AM_ERROR_INVALID_ARG; }

    try {
        std::vector<agent::Message> messages = toMgr(mgr)->drainInbox(agent_id);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& msg : messages) {
            arr.push_back({
                {"from_id",   msg.from_id},
                {"to_id",     msg.to_id},
                {"payload",   msg.payload},
                {"timestamp", msg.timestamp}
            });
        }
        return writeJson(arr.dump(), out_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_drain_inbox: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_drain_inbox: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Pattern C — blackboard
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_blackboard_write(AgentManager* mgr, const char* key,
                                 const char* value_json)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!key)             { setError("am_blackboard_write: key is null"); return AM_ERROR_INVALID_ARG; }
    if (!value_json)      { setError("am_blackboard_write: value_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json value;
        if (!parseJson(value_json, value, "am_blackboard_write")) return AM_ERROR_INVALID_ARG;

        toMgr(mgr)->blackboardWrite(key, std::move(value));
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_blackboard_write: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_blackboard_write: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_blackboard_read(AgentManager* mgr, const char* key,
                                char* out_value_json, size_t out_size)
{
    if (!requireMgr(mgr))   return AM_ERROR_INVALID_ARG;
    if (!key)               { setError("am_blackboard_read: key is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json value = toMgr(mgr)->blackboardRead(key);
        return writeJson(value.dump(), out_value_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_blackboard_read: ") + e.what());
        std::string msg = e.what();
        if (msg.find("not found") != std::string::npos ||
            msg.find("No entry")  != std::string::npos ||
            msg.find("missing")   != std::string::npos) {
            return AM_ERROR_NOT_FOUND;
        }
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_blackboard_read: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_blackboard_keys(AgentManager* mgr, const char* prefix,
                                char* out_json, size_t out_size)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;

    try {
        std::string pfx = (prefix ? prefix : "");
        std::vector<std::string> keys = toMgr(mgr)->blackboardKeys(pfx);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& k : keys) arr.push_back(k);
        return writeJson(arr.dump(), out_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_blackboard_keys: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_blackboard_keys: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Composition — fan-out / fan-in
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_fan_out(AgentManager* mgr, const char* configs_json_array,
                        const char* shared_task,
                        AgentFuture*** out_futures, size_t* out_count)
{
    if (!requireMgr(mgr))     return AM_ERROR_INVALID_ARG;
    if (!configs_json_array)  { setError("am_fan_out: configs_json_array is null"); return AM_ERROR_INVALID_ARG; }
    if (!shared_task)         { setError("am_fan_out: shared_task is null"); return AM_ERROR_INVALID_ARG; }
    if (!out_futures)         { setError("am_fan_out: out_futures is null"); return AM_ERROR_INVALID_ARG; }
    if (!out_count)           { setError("am_fan_out: out_count is null");   return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(configs_json_array, j, "am_fan_out")) return AM_ERROR_INVALID_ARG;
        if (!j.is_array()) {
            setError("am_fan_out: configs_json_array must be a JSON array");
            return AM_ERROR_INVALID_ARG;
        }

        std::vector<agent::AgentConfig> configs;
        configs.reserve(j.size());
        for (const auto& item : j) {
            agent::AgentConfig cfg;
            if (item.contains("name")           && item["name"].is_string())            cfg.name           = item["name"].get<std::string>();
            if (item.contains("task")           && item["task"].is_string())            cfg.task           = item["task"].get<std::string>();
            if (item.contains("max_iterations") && item["max_iterations"].is_number_integer()) cfg.max_iterations = item["max_iterations"].get<int>();
            if (item.contains("max_depth")      && item["max_depth"].is_number_integer())      cfg.max_depth      = item["max_depth"].get<int>();
            if (item.contains("extra"))                                                        cfg.extra          = item["extra"];
            if (item.contains("user_id") && item["user_id"].is_string()) {
                if (!cfg.extra.is_object()) cfg.extra = nlohmann::json::object();
                cfg.extra["user_id"] = item["user_id"].get<std::string>();
            }
            configs.push_back(std::move(cfg));
        }

        std::string task_str = shared_task;
        std::vector<std::future<nlohmann::json>> futures =
            toMgr(mgr)->fanOut(configs, task_str);

        size_t count = futures.size();

        // Allocate the output array.  Caller frees via am_fan_out_free_array().
        auto** arr = new AgentFuture*[count];
        for (size_t i = 0; i < count; ++i) {
            auto* af = new AgentFuture_{std::move(futures[i]), ""};
            arr[i] = reinterpret_cast<AgentFuture*>(af);
        }

        *out_futures = arr;
        *out_count   = count;
        return AM_OK;

    } catch (const std::exception& e) {
        setError(std::string("am_fan_out: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_fan_out: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

void am_fan_out_free_array(AgentFuture** arr) {
    delete[] arr;
}

am_status_t am_research_from_angles(AgentManager* mgr,
                                     const char* angles_json_array,
                                     const char* topic,
                                     char* out_result_json, size_t out_size)
{
    if (!requireMgr(mgr))       return AM_ERROR_INVALID_ARG;
    if (!angles_json_array)     { setError("am_research_from_angles: angles_json_array is null"); return AM_ERROR_INVALID_ARG; }
    if (!topic)                 { setError("am_research_from_angles: topic is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(angles_json_array, j, "am_research_from_angles")) return AM_ERROR_INVALID_ARG;
        if (!j.is_array()) {
            setError("am_research_from_angles: angles_json_array must be a JSON array");
            return AM_ERROR_INVALID_ARG;
        }

        std::vector<std::string> angles;
        angles.reserve(j.size());
        for (const auto& item : j) {
            if (item.is_string()) {
                angles.push_back(item.get<std::string>());
            }
        }

        nlohmann::json result = toMgr(mgr)->researchFromAngles(angles, topic);
        return writeJson(result.dump(), out_result_json, out_size);

    } catch (const std::exception& e) {
        setError(std::string("am_research_from_angles: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_research_from_angles: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Real-time injection
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_inject_work(AgentManager* mgr, const char* agent_id,
                            const char* work_item_json)
{
    if (!requireMgr(mgr))   return AM_ERROR_INVALID_ARG;
    if (!agent_id)          { setError("am_inject_work: agent_id is null"); return AM_ERROR_INVALID_ARG; }
    if (!work_item_json)    { setError("am_inject_work: work_item_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(work_item_json, j, "am_inject_work")) return AM_ERROR_INVALID_ARG;

        // Required fields.
        if (!j.contains("name") || !j["name"].is_string()) {
            setError("am_inject_work: work_item_json must contain string field 'name'");
            return AM_ERROR_INVALID_ARG;
        }
        if (!j.contains("id") || !j["id"].is_string()) {
            setError("am_inject_work: work_item_json must contain string field 'id'");
            return AM_ERROR_INVALID_ARG;
        }

        std::string name = j["name"].get<std::string>();
        std::string id   = j["id"].get<std::string>();
        nlohmann::json inputs = j.value("inputs", nlohmann::json::object());

        // Optional position hint — the inject call uses Front by default.
        // (AgentManager::injectWork routes to AgentContext::injectFromOutside
        //  which always pushes to the Front; the JSON hint is informational.)
        // If the caller requests "back" we note it but honour what the
        // manager implements.
        std::string position = j.value("position", "front");
        (void)position; // AgentManager::injectWork picks the position internally.

        std::unique_ptr<agent::WorkItem> item =
            toMgr(mgr)->factory().create(name, id, inputs);

        toMgr(mgr)->injectWork(agent_id, std::move(item));
        return AM_OK;

    } catch (const std::exception& e) {
        setError(std::string("am_inject_work: ") + e.what());
        std::string msg = e.what();
        if (msg.find("not registered") != std::string::npos ||
            msg.find("Unknown")        != std::string::npos) {
            return AM_ERROR_NOT_FOUND;
        }
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_inject_work: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Events
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_subscribe_events(AgentManager* mgr, am_event_cb cb,
                                 void* user_data)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!cb)              { setError("am_subscribe_events: cb is null"); return AM_ERROR_INVALID_ARG; }

    try {
        // Capture the C callback and user_data by value.
        // The C function pointer itself serves as the subscription key for
        // unsubscription (user_data is passed back to the callback but not used
        // as the key, matching the unsubscribe API which takes only cb).
        agent::EventCallback lambda = [cb, user_data](const nlohmann::json& event) {
            try {
                std::string event_str = event.dump();
                cb(event_str.c_str(), user_data);
            } catch (...) {
                // Never let a C callback exception propagate into the event bus.
            }
        };

        // Use the C function pointer value as the opaque key.
        toMgr(mgr)->subscribeEvents(std::move(lambda),
                                    reinterpret_cast<void*>(cb));
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_subscribe_events: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_subscribe_events: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_unsubscribe_events(AgentManager* mgr, am_event_cb cb) {
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!cb)              { setError("am_unsubscribe_events: cb is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->unsubscribeEvents(reinterpret_cast<void*>(cb));
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_unsubscribe_events: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_unsubscribe_events: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Hot reload / config
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_reload_prompts(AgentManager* mgr) {
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;

    try {
        toMgr(mgr)->reloadPrompts();
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_reload_prompts: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_reload_prompts: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_set_prompts_dir(AgentManager* mgr, const char* dir_path) {
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!dir_path)        { setError("am_set_prompts_dir: dir_path is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->setPromptsDir(std::filesystem::path(dir_path));
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_set_prompts_dir: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_set_prompts_dir: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_set_user_quota(AgentManager* mgr, const char* user_id,
                               const char* quota_json)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;
    if (!user_id)         { setError("am_set_user_quota: user_id is null"); return AM_ERROR_INVALID_ARG; }
    if (!quota_json)      { setError("am_set_user_quota: quota_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(quota_json, j, "am_set_user_quota")) return AM_ERROR_INVALID_ARG;

        agent::UserQuota quota;
        if (j.contains("max_concurrent_agents") && j["max_concurrent_agents"].is_number_integer()) {
            quota.max_concurrent_agents = j["max_concurrent_agents"].get<int>();
        }
        if (j.contains("max_llm_inflight") && j["max_llm_inflight"].is_number_integer()) {
            quota.max_llm_inflight = j["max_llm_inflight"].get<int>();
        }
        if (j.contains("max_tool_inflight") && j["max_tool_inflight"].is_number_integer()) {
            quota.max_tool_inflight = j["max_tool_inflight"].get<int>();
        }

        toMgr(mgr)->setUserQuota(user_id, quota);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_set_user_quota: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_set_user_quota: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// MCP management
// ═══════════════════════════════════════════════════════════════════════════════

am_status_t am_connect_mcp(AgentManager* mgr, const char* server_config_json) {
    if (!requireMgr(mgr))       return AM_ERROR_INVALID_ARG;
    if (!server_config_json)    { setError("am_connect_mcp: server_config_json is null"); return AM_ERROR_INVALID_ARG; }

    try {
        nlohmann::json j;
        if (!parseJson(server_config_json, j, "am_connect_mcp")) return AM_ERROR_INVALID_ARG;

        agent::MCPServerConfig cfg;
        if (j.contains("name") && j["name"].is_string()) cfg.name  = j["name"].get<std::string>();
        if (j.contains("url")  && j["url"].is_string())  cfg.url   = j["url"].get<std::string>();
        if (j.contains("extra"))                          cfg.extra = j["extra"];

        if (cfg.name.empty()) {
            setError("am_connect_mcp: server_config_json must contain 'name'");
            return AM_ERROR_INVALID_ARG;
        }

        toMgr(mgr)->connectMCP(cfg);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_connect_mcp: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_connect_mcp: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_disconnect_mcp(AgentManager* mgr, const char* server_name) {
    if (!requireMgr(mgr))  return AM_ERROR_INVALID_ARG;
    if (!server_name)      { setError("am_disconnect_mcp: server_name is null"); return AM_ERROR_INVALID_ARG; }

    try {
        toMgr(mgr)->disconnectMCP(server_name);
        return AM_OK;
    } catch (const std::exception& e) {
        setError(std::string("am_disconnect_mcp: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_disconnect_mcp: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

am_status_t am_list_mcp_servers(AgentManager* mgr,
                                 char* out_json, size_t out_size)
{
    if (!requireMgr(mgr)) return AM_ERROR_INVALID_ARG;

    try {
        nlohmann::json result = toMgr(mgr)->listMCPServers();
        return writeJson(result.dump(), out_json, out_size);
    } catch (const std::exception& e) {
        setError(std::string("am_list_mcp_servers: ") + e.what());
        return AM_ERROR_INTERNAL;
    } catch (...) {
        setError("am_list_mcp_servers: unknown exception");
        return AM_ERROR_INTERNAL;
    }
}

} // extern "C"
