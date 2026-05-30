# C ABI Guide for GUI Authors

This guide explains how to use `libagent_engine.so` from C or C++ GUI code via
the stable C ABI defined in `agent_engine/c_api.h`.

---

## Include Path and Linking

```cmake
# CMakeLists.txt
target_include_directories(my_gui PRIVATE ${AGENT_ENGINE_INCLUDE_DIR})
target_link_libraries(my_gui PRIVATE agent_engine)
```

```c
#include "agent_engine/c_api.h"
```

The ABI is `extern "C"` — link with any C or C++ compiler.

---

## Complete Lifecycle Example

```c
#include "agent_engine/c_api.h"
#include <stdio.h>
#include <string.h>

#define BUF_SIZE 65536
static char result_buf[BUF_SIZE];

int main(void) {
    // 1. Create manager
    AgentManager* mgr = am_create(
        "{\"prompts_dir\":\"/path/to/prompts\","
        " \"thread_pool_size\":8,"
        " \"default_user_id\":\"gui_user\"}");
    if (!mgr) {
        fprintf(stderr, "Failed: %s\n", am_last_error(NULL));
        return 1;
    }

    // 2. Spawn an agent
    char agent_id[256] = {0};
    am_status_t st = am_spawn_agent(mgr,
        "{\"name\":\"MyAgent\","
        " \"user_id\":\"gui_user\","
        " \"max_iterations\":50}",
        agent_id, sizeof(agent_id));
    if (st != AM_OK) {
        fprintf(stderr, "Spawn failed: %s\n", am_last_error(mgr));
        am_destroy(mgr);
        return 1;
    }

    // 3. Run agent asynchronously
    AgentFuture* fut = am_run_agent(mgr, agent_id,
        "{\"task\":\"Summarise the quarterly report\"}");
    if (!fut) {
        fprintf(stderr, "Run failed: %s\n", am_last_error(mgr));
        am_destroy_agent(mgr, agent_id);
        am_destroy(mgr);
        return 1;
    }

    // 4. Wait for result (30 second timeout)
    st = am_future_wait(fut, 30000, result_buf, BUF_SIZE);
    am_future_free(fut);   // always free the future

    if (st == AM_ERROR_TIMEOUT) {
        fprintf(stderr, "Agent timed out — cancelling.\n");
        am_cancel_agent(mgr, agent_id);
    } else if (st == AM_OK) {
        printf("Result: %s\n", result_buf);
    }

    // 5. Cleanup
    am_destroy_agent(mgr, agent_id);
    am_destroy(mgr);
    return 0;
}
```

---

## Buffer-Out Pattern

All functions that return JSON data use a caller-provided `(buf, size)` pair.
If the output fits, `buf` is written and `AM_OK` is returned. If the output
would not fit, `AM_ERROR_BUFFER_TOO_SMALL` is returned and `buf` is left
in an unspecified state.

**Retry loop pattern:**

```c
size_t buf_size = 4096;
char*  buf      = malloc(buf_size);

while (1) {
    am_status_t st = am_blackboard_read(mgr, key, buf, buf_size);
    if (st == AM_OK) break;
    if (st == AM_ERROR_BUFFER_TOO_SMALL) {
        buf_size *= 2;
        buf = realloc(buf, buf_size);
        continue;
    }
    // Other error — handle it
    fprintf(stderr, "Error: %s\n", am_last_error(mgr));
    break;
}

// use buf …
free(buf);
```

---

## Event Callback Threading Contract

```c
void my_event_handler(const char* event_json, void* user_data) {
    // WARNING: called on an engine (pool) thread.
    // NEVER touch UI objects directly here.
    // Marshal to the UI thread instead, e.g.:
    dispatch_to_ui_thread(event_json);  // your framework's mechanism
}

am_subscribe_events(mgr, my_event_handler, NULL);
```

Event types emitted (subset):
- `agent_spawned`, `agent_started`, `agent_finished`, `agent_cancelled`,
  `agent_destroyed`
- `work_item_started`, `work_item_finished`
- `batch_started`, `batch_finished`
- `work_injected`
- `message_sent`, `message_received`
- `blackboard_updated`
- `prompts_reloaded`
- `quota_exceeded`

Each event JSON contains at minimum:
```json
{"type": "agent_started", "timestamp": "2025-01-01T00:00:00Z", "agent_id": "agent_1"}
```

---

## All Status Codes

| Code | Value | Meaning |
|------|-------|---------|
| `AM_OK`                       | 0  | Success |
| `AM_ERROR_INVALID_ARG`        | 1  | NULL pointer or invalid JSON argument |
| `AM_ERROR_NOT_FOUND`          | 2  | Agent ID, key, or resource not found |
| `AM_ERROR_INTERNAL`           | 3  | Unexpected internal error (check `am_last_error`) |
| `AM_ERROR_TIMEOUT`            | 4  | `am_future_wait` deadline exceeded |
| `AM_ERROR_BUFFER_TOO_SMALL`   | 5  | Output buffer too small; retry with larger buffer |
| `AM_ERROR_NOT_INITIALIZED`    | 6  | Manager was not created successfully |
| `AM_ERROR_DEPRECATED`         | 7  | Function is deprecated in this ABI version |
| `AM_ERROR_CANCELLED`          | 8  | Operation was cancelled |
| `AM_ERROR_PROMPT_NOT_FOUND`   | 9  | Template file not found in prompts directory |
| `AM_ERROR_PROMPT_SUBSTITUTION`| 10 | Template contains an unresolved `{{PLACEHOLDER}}` |
| `AM_ERROR_QUOTA_EXCEEDED`     | 11 | User's agent/LLM/tool quota is exhausted |
| `AM_ERROR_DEPENDENCY_CYCLE`   | 12 | Work item batch contains a dependency cycle |

---

## Pattern A — Run + Future + Pipe

```c
// Spawn researcher and writer
char rid[256]={}, wid[256]={};
am_spawn_agent(mgr, "{\"name\":\"researcher\"}", rid, sizeof(rid));
am_spawn_agent(mgr, "{\"name\":\"writer\"}",     wid, sizeof(wid));

// Register pipe: when researcher finishes, its output is templated into
// writer's task via {{OUTPUT}} substitution.
am_pipe(mgr, rid, wid, "Based on: {{OUTPUT}}, write a 200-word summary.");

// Run researcher asynchronously
AgentFuture* rfut = am_run_agent(mgr, rid,
    "{\"task\":\"Research the EU AI Act\"}");

// While researcher runs, do other UI work ...

// Wait for researcher result
char rbuf[65536];
am_status_t rs = am_future_wait(rfut, 30000, rbuf, sizeof(rbuf));
am_future_free(rfut);

// Pipe fires automatically after researcher finishes.
// Drain writer's inbox to see the pipe message:
char ibuf[65536];
am_drain_inbox(mgr, wid, ibuf, sizeof(ibuf));

// Optionally run writer now
AgentFuture* wfut = am_run_agent(mgr, wid, "{\"task\":\"write summary\"}");
am_future_wait(wfut, 30000, rbuf, sizeof(rbuf));
am_future_free(wfut);
```

---

## Pattern B — Messaging

```c
char coord_id[256]={}, worker_id[256]={};
am_spawn_agent(mgr, "{\"name\":\"coordinator\"}", coord_id, sizeof(coord_id));
am_spawn_agent(mgr, "{\"name\":\"worker\"}",      worker_id, sizeof(worker_id));

// Coordinator sends a message to worker
am_send_message(mgr, coord_id, worker_id,
    "{\"type\":\"task\",\"payload\":\"process document X\"}");

// Broadcast to all other agents
am_broadcast(mgr, coord_id,
    "{\"type\":\"status_update\",\"msg\":\"starting batch 2\"}");

// Worker drains its inbox
char inbox_buf[65536];
am_status_t st = am_drain_inbox(mgr, worker_id, inbox_buf, sizeof(inbox_buf));
if (st == AM_OK) {
    // inbox_buf contains a JSON array of Message objects:
    // [{"from_id":"agent_1","to_id":"agent_2","payload":{...},"timestamp":"..."}]
    printf("Messages: %s\n", inbox_buf);
}
```

---

## Pattern C — Blackboard

```c
// Write findings
am_blackboard_write(mgr, "findings/legal",     "{\"text\":\"legal analysis\"}");
am_blackboard_write(mgr, "findings/technical", "{\"text\":\"tech analysis\"}");
am_blackboard_write(mgr, "findings/market",    "{\"text\":\"market analysis\"}");

// Read a specific key
char val_buf[4096];
am_blackboard_read(mgr, "findings/legal", val_buf, sizeof(val_buf));
printf("Legal: %s\n", val_buf);

// List all keys with a prefix
char keys_buf[4096];
am_blackboard_keys(mgr, "findings/", keys_buf, sizeof(keys_buf));
// keys_buf: ["findings/legal","findings/market","findings/technical"]
printf("Keys: %s\n", keys_buf);
```

---

## Hot Reload Workflow

```c
// 1. Edit template files on disk (e.g. /prompts/reason_stage.md)
// 2. Call am_reload_prompts to clear the in-memory cache:
am_reload_prompts(mgr);
// 3. Next stage execution will re-read the updated template from disk.

// To change the prompts directory at runtime:
am_set_prompts_dir(mgr, "/path/to/new_prompts");
// This implies am_reload_prompts automatically.
```

---

## Multi-Tenancy: Per-User Spawning and Quota Configuration

```c
// Configure quota for a user
am_set_user_quota(mgr, "alice",
    "{\"max_concurrent_agents\":5,"
    " \"max_llm_inflight\":2,"
    " \"max_tool_inflight\":10}");

// Spawn agents for alice — each spawn decrements her agent counter
char id1[256]={}, id2[256]={};
am_spawn_agent(mgr,
    "{\"user_id\":\"alice\",\"name\":\"Agent1\"}", id1, sizeof(id1));

// When alice's quota is exhausted:
am_status_t st = am_spawn_agent(mgr,
    "{\"user_id\":\"alice\",\"name\":\"TooMany\"}", id2, sizeof(id2));
if (st == AM_ERROR_QUOTA_EXCEEDED) {
    printf("Quota exceeded: %s\n", am_last_error(mgr));
}

// Release a slot by destroying an agent
am_destroy_agent(mgr, id1);  // alice now has one free slot again
```

---

## Error Handling: am_last_error()

`am_last_error()` returns the most recent error string for the **calling
thread**. The pointer is valid until the next ABI call on that thread.

```c
AgentManager* mgr = am_create("{bad json}");
if (!mgr) {
    // mgr is NULL; pass NULL to am_last_error
    fprintf(stderr, "Create error: %s\n", am_last_error(NULL));
    return 1;
}

char id[256];
am_status_t st = am_spawn_agent(mgr, "{bad}", id, sizeof(id));
if (st != AM_OK) {
    // mgr is valid; pass mgr for context (currently same as NULL for error string)
    fprintf(stderr, "Spawn error: %s\n", am_last_error(mgr));
}
```

---

## ABI Version Check

```c
int version = am_api_version();
if (version < 1) {
    fprintf(stderr, "Unsupported ABI version %d\n", version);
    return 1;
}
// Current version is 1 (requirements 1–60, worked examples 1–14)
```

Future ABI versions will be backwards compatible; new functions may be added
but existing function signatures will not change within the same major version.
