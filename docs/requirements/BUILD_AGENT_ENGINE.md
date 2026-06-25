# Build Prompt — C++ Agent Engine + Agent Server

You are building two C++ components from scratch:

1. **`agent/`** — A shared library (`libagent_engine.so`) that runs AI agents
2. **`agent_server_cpp/`** — An HTTP/WebSocket server that wraps the library and exposes it over a network

Nothing else. Do not build the UI, auth server, or MCP server. Focus entirely on these two.

---

## What the system does

An **agent** is a named process that receives a text prompt, calls an AI model (Anthropic, OpenAI, etc.) through a configurable pipeline, executes tool actions (bash, file read/write, web search, etc.), and emits the result. Multiple agents can run concurrently, share a key-value blackboard, send messages to each other, and pipe their output into each other.

The **agent engine** is the in-process library that does all of this. The **agent server** is the thin HTTP/WebSocket wrapper that lets external clients (a web UI, CLI, or test script) create agents, run them, and receive events.

---

## Component 1 — Agent Engine (`agent/`)

### Build system

File: `agent/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.17)
project(agent_engine VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

option(AGENT_ENABLE_API_LLM "Enable cloud LLM providers (requires OpenSSL)" ON)

# Collect all sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_library(agent_engine SHARED ${SOURCES})
target_include_directories(agent_engine PUBLIC include PRIVATE src)

find_package(nlohmann_json REQUIRED)
target_link_libraries(agent_engine PRIVATE nlohmann_json::nlohmann_json)

if(AGENT_ENABLE_API_LLM)
  find_package(OpenSSL REQUIRED)
  target_link_libraries(agent_engine PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(agent_engine PRIVATE AGENT_ENABLE_API_LLM)
endif()
```

Build command:
```bash
cmake -B agent/build -DAGENT_ENABLE_API_LLM=ON
cmake --build agent/build
```

### Directory layout

```
agent/
├── include/
│   └── agent_engine/
│       └── c_api.h          ← only public header, stable C ABI
├── src/
│   ├── c_api/
│   │   └── c_api.cpp        ← implements every function in c_api.h
│   └── agent/
│       ├── agent_manager.cpp
│       ├── llm_factory.cpp
│       ├── event_bus.cpp
│       ├── blackboard.cpp
│       ├── work_item.cpp
│       ├── quota.cpp
│       ├── thread_pool.cpp
│       ├── messaging.cpp
│       ├── actions/         ← one .cpp per action (14 total)
│       └── stages/          ← one .cpp per pipeline stage (10 total)
└── tests/
```

---

### The public C API — `agent/include/agent_engine/c_api.h`

Declare every function below with `extern "C"`. This is the only file external code may include.

**Rules:**
- All strings are null-terminated UTF-8 `char*`
- All strings the engine returns are heap-allocated — caller frees with `am_free_string()`
- All JSON inputs and outputs use the schemas in the Data Models section

```c
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Memory
void  am_free_string(char* str);

// Manager lifecycle
void* am_create();
void  am_destroy(void* handle);

// Agent CRUD
// config_json: { name, role, system_prompt, provider, model, api_key, parent_id }
// Returns: heap-allocated agent ID string, or NULL on failure
char* am_spawn_agent(void* handle, const char* config_json);
void  am_destroy_agent(void* handle, const char* agent_id);
char* am_list_agents(void* handle);           // JSON array of Agent objects
char* am_get_status(void* handle, const char* agent_id); // JSON Agent object

// Execution
void* am_run_agent(void* handle, const char* agent_id, const char* prompt); // returns future
// Blocks until done. Returns: { result, duration_ms, prompt_tokens, completion_tokens, error }
char* am_future_wait(void* future_handle);
// override_json: { prompt?, context?, priority? }
void  am_inject_work(void* handle, const char* agent_id, const char* override_json);
void  am_cancel_agent(void* handle, const char* agent_id);

// Wiring — connect output of one agent to input of another
void  am_pipe(void* handle, const char* from_id, const char* to_id);
void  am_unpipe(void* handle, const char* from_id, const char* to_id);

// Messaging
// message_json: { content, type, metadata }
void  am_send_message(void* handle, const char* from_id, const char* to_id, const char* message_json);
void  am_broadcast(void* handle, const char* from_id, const char* message_json);

// Shared blackboard (key-value store visible to all agents)
void  am_blackboard_write(void* handle, const char* key, const char* value_json);
char* am_blackboard_read(void* handle, const char* key);  // JSON value or null
char* am_blackboard_keys(void* handle);                   // JSON array of strings

// Events — callback fires on engine worker threads
typedef void (*am_event_callback_t)(const char* event_json, void* user_data);
int   am_subscribe_events(void* handle, am_event_callback_t callback, void* user_data);
void  am_unsubscribe_events(void* handle, int subscription_id);

// MCP tool servers
void  am_connect_mcp(void* handle, const char* server_url, const char* auth_token);
void  am_disconnect_mcp(void* handle, const char* server_name);
char* am_list_mcp_servers(void* handle); // JSON array: [{name, url, tool_count, tools}]

// LLM configuration — config_json: { provider, model, api_key, base_url? }
void  am_configure_llm(void* handle, const char* config_json);

#ifdef __cplusplus
}
#endif
```

---

### Core implementation — `agent/src/agent/agent_manager.cpp`

`AgentManager` is the central object. It owns:
- A map of `agent_id → Agent` objects
- A `ThreadPool` for async agent runs
- An `EventBus` for publishing events
- A `Blackboard` for shared state
- A map of `subscription_id → callback` for event subscribers
- A map of `from_id → [to_id, ...]` for pipe connections

Key behaviors:

**`am_spawn_agent`**: Parse config JSON, assign a UUID v4, set status to `idle`, store in the map, return the ID.

**`am_run_agent`**: Set status to `running`, emit `agent_started`, submit a `WorkItem` to the thread pool, return an opaque future handle. The thread pool picks it up, runs the 9-stage pipeline, resolves the future.

**`am_cancel_agent`**: Set a cancellation flag on the agent. The pipeline checks this flag between stages and stops cleanly. Emit `agent_cancelled`.

**`am_pipe`**: Add `to_id` to the pipe list for `from_id`. After an agent finishes, its result is automatically submitted as input to all piped agents.

**Blackboard writes** must emit a `blackboard_updated` event.

---

### Agent execution pipeline — `agent/src/agent/stages/`

Every agent run passes through exactly these stages in order. Each stage receives the `WorkItem` and may modify it before passing to the next.

| File | Stage | What it does |
|------|-------|-------------|
| `observe_stage.cpp` | observe | Collect all available context: messages in the agent's inbox, pipe inputs, blackboard snapshot, conversation history |
| `understand_stage.cpp` | understand | Parse and classify the incoming prompt — what kind of task is this? |
| `read_stage.cpp` | read | Pull relevant data from conversation history and any memory store |
| `code_intel_stage.cpp` | code_intel | If the context contains code, analyze structure: file paths, function names, symbols |
| `locate_stage.cpp` | locate | Identify which tools and MCP capabilities are relevant to this task |
| `inject_stage.cpp` | inject | Build the final prompt: combine system prompt, role constraints, context, and tool list |
| `reason_stage.cpp` | reason | Call the LLM provider with the assembled prompt; parse the response into action directives |
| `transform_stage.cpp` | transform | Post-process the LLM output: strip formatting artifacts, normalize whitespace |
| `validate_stage.cpp` | validate | Check output quality; re-enter at `reason_stage` if invalid (up to 3 retries) |
| `respond_stage.cpp` | respond | Emit `agent_finished` event; feed any connected pipes with the result |

The pipeline must check `agent.cancelled` between every stage and short-circuit to `respond_stage` if set.

---

### Action types — `agent/src/agent/actions/`

During `reason_stage`, parse the LLM response to extract action directives. Each action is synchronous; execute it and add the result to the `WorkItem` context before calling the LLM again.

| File | What it does |
|------|-------------|
| `bash_action.cpp` | Run a shell command in a subprocess with restricted env; capture stdout/stderr |
| `read_action.cpp` | Read a file from disk; return its contents |
| `write_action.cpp` | Write content to a file |
| `edit_action.cpp` | Apply a targeted replacement to an existing file |
| `glob_action.cpp` | Return all file paths matching a glob pattern |
| `grep_action.cpp` | Search file contents with regex; return matching lines and line numbers |
| `web_fetch_action.cpp` | HTTP GET/POST to an external URL; return response body |
| `web_search_action.cpp` | Call a configured search API; return structured results |
| `mcp_tool_action.cpp` | Build a JSON-RPC 2.0 request; send to the connected MCP server via HTTP POST /rpc |
| `messaging_actions.cpp` | Call `am_send_message` or `am_broadcast` on the manager |
| `blackboard_actions.cpp` | Call `am_blackboard_read` or `am_blackboard_write` |
| `memory_actions.cpp` | Read from or append to conversation history |
| `task_action.cpp` | Spawn a sub-task by calling `am_run_agent` + `am_future_wait` |
| `todo_write_action.cpp` | Write a TODO item to `~/.agent_todos/<agent_id>.json` |

---

### LLM factory — `agent/src/agent/llm_factory.cpp`

`makeLLMClientFromConfig(config_json)` reads the `"provider"` field and returns a provider-specific client. Every client must implement:

```cpp
class LLMClient {
public:
    virtual ~LLMClient() = default;
    // Calls the provider and returns the response text
    virtual std::string complete(const std::string& prompt) = 0;
    // Returns live model list for this provider (used by GET /api/llm/models)
    virtual std::vector<std::string> listModels() = 0;
};
```

Support exactly these 16 providers:

| `provider` value | Notes |
|-----------------|-------|
| `anthropic` | Messages API, streaming |
| `openai` | Chat completions, streaming |
| `google` | Gemini API |
| `groq` | OpenAI-compatible |
| `mistral` | Native Mistral API |
| `deepseek` | OpenAI-compatible |
| `xai` | Grok API |
| `openrouter` | OpenAI-compatible; model names are `provider/model` |
| `together` | OpenAI-compatible |
| `ollama` | REST at `base_url` (default `http://localhost:11434`) |
| `llamacpp` | REST at `base_url` |
| `lmstudio` | OpenAI-compatible at `base_url` |
| `vllm` | OpenAI-compatible at `base_url` |
| `llama` | Direct llama.cpp |
| `custom` | OpenAI-compatible at user-supplied `base_url` |
| `mock` | Returns deterministic canned responses; no network |

LLM call retry policy: up to 3 retries, exponential backoff — 1 s, 2 s, 4 s.

---

### Event bus — `agent/src/agent/event_bus.cpp`

Thread-safe pub/sub. Subscriptions are a map of `int → callback`. Publishing serializes the event to JSON and calls every registered callback.

**Callbacks fire on engine worker threads.** Callers (the Agent Server) are responsible for not touching shared state directly in the callback — they must push the JSON string into their own thread-safe queue.

Emit exactly these event types:

| Event type | When | Payload |
|-----------|------|---------|
| `agent_started` | Agent begins execution | `agent_id` |
| `work_item_started` | Agent begins processing a work item | `agent_id`, `work_item_id`, `prompt` |
| `agent_finished` | Agent completes | `agent_id`, `result`, `duration_ms` |
| `agent_failed` | Unrecoverable error | `agent_id`, `error`, `code` |
| `agent_cancelled` | Agent was cancelled | `agent_id` |
| `blackboard_updated` | Any blackboard write | `key`, `value` |
| `mcp_connected` | MCP server attached | `server_name`, `tool_count` |
| `mcp_disconnected` | MCP server removed | `server_name` |
| `quota_exceeded` | Provider rate limit hit | `agent_id`, `provider`, `limit_type` |

Every event JSON envelope:
```json
{
  "event": "<event_type>",
  "payload": { ... },
  "timestamp": "2026-06-25T10:00:00Z"
}
```

---

### Blackboard — `agent/src/agent/blackboard.cpp`

Key-value store. Keys are strings; values are JSON (stored as serialized strings).

```cpp
class Blackboard {
    std::mutex mtx;
    std::unordered_map<std::string, std::string> data;
public:
    void write(const std::string& key, const std::string& value_json);
    std::string read(const std::string& key); // returns "" if not found
    std::vector<std::string> keys();
};
```

Every `write()` must emit `blackboard_updated` on the event bus.

---

### Thread pool — `agent/src/agent/thread_pool.cpp`

Standard fixed-size thread pool. Default thread count: `std::thread::hardware_concurrency()`, minimum 4.

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t n_threads);
    ~ThreadPool(); // joins all threads
    std::future<std::string> submit(std::function<std::string()> task);
};
```

---

## Component 2 — Agent Server (`agent_server_cpp/`)

### What it is

A thin HTTP + WebSocket server. It embeds `libagent_engine` in-process via the C API and exposes every engine function over a network. No business logic — just translation from HTTP to C API calls.

### Build system

File: `agent_server_cpp/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.17)
project(agent_server)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(agent_server
    src/main.cpp
    src/router.cpp
    src/auth.cpp
    src/ws_hub.cpp
)

target_include_directories(agent_server PRIVATE
    src
    ../agent/include
    third_party/cpp-httplib
)

target_link_libraries(agent_server PRIVATE
    ${CMAKE_SOURCE_DIR}/../agent/build/libagent_engine.so
    OpenSSL::SSL OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

Build command:
```bash
cmake -B agent_server_cpp/build
cmake --build agent_server_cpp/build
```

Output binary: `agent_server_cpp/bin/agent_server`

### Directory layout

```
agent_server_cpp/
├── src/
│   ├── main.cpp      ← startup, engine init, register routes, start server
│   ├── router.cpp    ← all HTTP route handlers
│   ├── auth.cpp      ← token introspection + 60-second cache
│   └── ws_hub.cpp    ← WebSocket client list + sender thread
├── third_party/
│   └── cpp-httplib/  ← header-only HTTP library (include httplib.h)
└── CMakeLists.txt
```

### Startup sequence (`main.cpp`)

```
1. Parse CLI args: --host (default 0.0.0.0), --port (default 3002)
2. Read env vars: AUTH_SERVER_URL, JUDGE_PROVIDER, JUDGE_MODEL, JUDGE_API_KEY
3. Call am_create() → store as global engine handle
4. Call am_subscribe_events() → callback pushes event JSON into ws_hub queue
5. Register all HTTP routes (see below)
6. Start httplib::Server
7. On SIGTERM/SIGINT: call am_destroy(), stop server
```

### Authentication — `auth.cpp`

Every request handler must call this before doing anything:

```
1. Extract token from "Authorization: Bearer <token>" header
2. Check in-memory cache (mutex-protected map<string, CacheEntry>)
   - If found and not expired (TTL 60s): return cached result
3. POST to AUTH_SERVER_URL + "/introspect" with body {"token": "<token>"}
4. Parse response:
   - "active": true  → cache the result, allow the request
   - "active": false → return HTTP 401 immediately
5. If the introspect call itself fails → return HTTP 401
```

Return 401 JSON: `{"error": "unauthorized"}`
Return 503 JSON: `{"error": "engine unavailable"}` if `am_create()` returned null.

### REST route handlers — `router.cpp`

Every handler: authenticate first, then call the engine, then return JSON.

---

**GET /health**
```json
{ "status": "ok", "version": "1.0.0" }
```
No auth required.

---

**GET /api/llm/models**

Call `listModels()` on a temporary `LLMClient` for each configured provider. Return:
```json
[
  { "provider": "anthropic", "models": ["claude-opus-4-8", "claude-sonnet-4-6"] },
  { "provider": "openai",    "models": ["gpt-4o", "gpt-4o-mini"] }
]
```

---

**POST /api/llm/configure**

Body: `{ "provider": "anthropic", "model": "claude-sonnet-4-6", "api_key": "..." }`

Calls: `am_configure_llm(handle, body_json)`

Response: `200 {"ok": true}`

---

**GET /api/agents**

Calls: `am_list_agents(handle)` → parse → return as JSON array.

---

**POST /api/agents**

Body: `{ "name", "role", "system_prompt", "provider", "model", "api_key", "parent_id" }`

Calls: `am_spawn_agent(handle, body_json)` → returns agent ID string.

Then calls `am_get_status(handle, agent_id)` and return the full Agent object.

Response: `201 Agent`

---

**GET /api/agents/{id}**

Calls: `am_get_status(handle, id)` → parse → return.

Response: `200 Agent`

---

**DELETE /api/agents/{id}**

Calls: `am_cancel_agent(handle, id)` then `am_destroy_agent(handle, id)`

Response: `204`

---

**POST /api/agents/{id}/run**

Body: `{ "prompt": "..." }`

```
1. future = am_run_agent(handle, id, prompt)
2. result_json = am_future_wait(future)  ← blocks on thread pool
3. return 200 result_json
```

Response: `200 { "result": "...", "duration_ms": 4200, "prompt_tokens": 100, "completion_tokens": 350, "error": null }`

---

**POST /api/agents/{id}/inject**

Body: `{ "prompt": "...", "context": "...", "priority": 1 }`

Calls: `am_inject_work(handle, id, body_json)`

Response: `200 {"ok": true}`

---

**POST /api/agents/{id}/cancel**

Calls: `am_cancel_agent(handle, id)`

Response: `200 {"ok": true}`

---

**POST /api/pipe**

Body: `{ "from_id": "...", "to_id": "..." }`

Calls: `am_pipe(handle, from_id, to_id)`

Response: `200 {"ok": true}`

---

**DELETE /api/pipe**

Body: `{ "from_id": "...", "to_id": "..." }`

Calls: `am_unpipe(handle, from_id, to_id)`

Response: `200 {"ok": true}`

---

**POST /api/agents/{from_id}/send**

Body: `{ "to_id": "...", "content": "...", "type": "text", "metadata": {} }`

Calls: `am_send_message(handle, from_id, to_id, message_json)`

Response: `200 {"ok": true}`

---

**POST /api/broadcast**

Body: `{ "from_id": "...", "content": "..." }`

Calls: `am_broadcast(handle, from_id, message_json)`

Response: `200 {"ok": true}`

---

**GET /api/blackboard**

Calls: `am_blackboard_keys(handle)`

Response: `200 {"keys": ["key1", "key2"]}`

---

**GET /api/blackboard/{key}**

Calls: `am_blackboard_read(handle, key)`

Response: `200 {"key": "...", "value": <any JSON value>}`

---

**POST /api/blackboard/{key}**

Body: `{ "value": <any JSON value> }`

Calls: `am_blackboard_write(handle, key, value_json)`

Response: `200 {"ok": true}`

---

**GET /api/mcp**

Calls: `am_list_mcp_servers(handle)`

Response: `200 [{name, url, tool_count, tools}]`

---

**POST /api/mcp**

Body: `{ "url": "http://localhost:8081", "auth_token": "..." }`

Calls: `am_connect_mcp(handle, url, auth_token)`

Response: `200 {"ok": true}`

---

**DELETE /api/mcp/{name}**

Calls: `am_disconnect_mcp(handle, name)`

Response: `200 {"ok": true}`

---

**POST /api/benchmark**

Body: `{ "agent_ids": ["id1", "id2"], "prompt": "..." }`

Implementation:
1. For each agent ID, call `am_run_agent` concurrently using `std::async`
2. Collect results via `am_future_wait`; measure wall-clock latency for each
3. For each result, call the judge LLM (configured via `JUDGE_PROVIDER`/`JUDGE_MODEL`/`JUDGE_API_KEY` env vars) with:

```
You are an impartial evaluator. Rate the quality of the following AI response
on a scale from 0 to 10 (0 = useless, 10 = perfect).

Question: <original prompt>
Response: <agent result>

Reply with only JSON: {"score": <number 0-10>}
```

4. Parse the score; default to 0 if parsing fails.
5. Return an array of BenchmarkResult objects (see Data Models section).

---

### WebSocket `/events` — `ws_hub.cpp`

The hub manages:
- A list of connected WebSocket client handles (protected by a mutex)
- A thread-safe `std::queue<std::string>` of pending event JSON strings
- A dedicated **sender thread** that runs continuously

**On engine event callback (fires on engine thread):**
```
Lock queue_mutex → push event_json → unlock → notify condition_variable
```

**Sender thread loop:**
```
Lock queue_mutex → drain queue into local vector → unlock
For each event JSON string:
    For each connected client:
        ws_send(client, event_json)
```

**On client connect:**
```
am_subscribe_events(handle, callback, &hub)
Add client to connected list
```

**On client disconnect:**
```
am_unsubscribe_events(handle, subscription_id)
Remove client from connected list
```

Every message is the raw event JSON from the engine:
```json
{
  "event": "<event_type>",
  "payload": { ... },
  "timestamp": "2026-06-25T10:00:00Z"
}
```

---

## Data Models

Use these exact JSON schemas everywhere.

### Agent

```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "name": "research-specialist",
  "user_id": "user@example.com",
  "role": "specialist",
  "system_prompt": "You are a research specialist...",
  "model": "claude-sonnet-4-6",
  "provider": "anthropic",
  "status": "idle",
  "parent_id": null,
  "metadata": {}
}
```

`status` enum: `idle` | `running` | `waiting` | `done` | `error` | `cancelled`
`role` enum: `orchestrator` | `worker` | `specialist` | `reviewer` | `planner`

### AgentGroup

```json
{
  "id": "uuid-v4",
  "name": "research-team",
  "mode": "sequential",
  "agents": ["agent-id-1", "agent-id-2"],
  "edges": { "agent-id-1": "agent-id-2" }
}
```

`mode` enum: `parallel` | `sequential` | `broadcast` | `consensus` | `pipeline`

### Task

```json
{
  "id": "uuid-v4",
  "prompt": "Research climate change impacts.",
  "target_id": "agent-id-1",
  "target_type": "agent",
  "status": "done",
  "started_at": "2026-06-25T10:00:00Z",
  "finished_at": "2026-06-25T10:00:45Z",
  "duration_ms": 45000,
  "result": "..."
}
```

### BenchmarkResult

```json
{
  "id": "uuid-v4",
  "agent_id": "agent-id-1",
  "agent_name": "GPT-4o Agent",
  "model": "gpt-4o",
  "provider": "openai",
  "prompt": "Explain quantum entanglement.",
  "speed":       { "latency_ms": 2500, "ttft_ms": 150, "tokens_per_sec": 35.2 },
  "cost":        { "prompt_tokens": 100, "completion_tokens": 250, "total_tokens": 350, "est_cost_usd": 0.00525 },
  "quality":     { "score": 8.7, "method": "llm_judge" },
  "reliability": { "success": true, "retries": 0, "error": null },
  "timestamp": "2026-06-25T10:05:00Z"
}
```

---

## Environment Variables

```bash
# Agent Server
AUTH_SERVER_URL=http://localhost:8080   # used for token introspection
AGENT_SERVER_PORT=3002
AGENT_SERVER_HOST=0.0.0.0

# Benchmark quality scoring (LLM-as-judge)
JUDGE_PROVIDER=anthropic
JUDGE_MODEL=claude-sonnet-4-6
JUDGE_API_KEY=sk-ant-...

# Default LLM for agents that don't specify their own
DEFAULT_PROVIDER=anthropic
DEFAULT_MODEL=claude-sonnet-4-6
DEFAULT_API_KEY=sk-ant-...
```

---

## Hard Requirements

These are correctness constraints, not suggestions. Violating any one of them is a bug.

| Constraint | Requirement |
|-----------|-------------|
| Agent spawn time | Under 100 ms |
| First token forwarded to caller | Under 500 ms after agent receives prompt (excluding AI provider network time) |
| Concurrent agents | At least 20 without degradation |
| WebSocket event delivery | Under 200 ms latency |
| LLM retry policy | Up to 3 retries, exponential backoff: 1 s, 2 s, 4 s |
| Token introspection cache TTL | 60 seconds |
| API keys | Never logged, never returned in API responses |
| C API ABI | Frozen — breaking changes require a major version bump |
| New LLM provider | Only `llm_factory.cpp` changes |
| WebSocket thread safety | Engine callbacks may never call `ws_send` directly |
| Bash action sandbox | Restricted filesystem; no outbound network by default |

---

## Verification

After building, verify end-to-end:

```bash
# 1. Start the server (assumes auth server is running or AUTH_SERVER_URL is optional)
./agent_server_cpp/bin/agent_server --port 3002

# 2. Check health
curl http://localhost:3002/health
# Expected: {"status":"ok","version":"1.0.0"}

# 3. Get a token (or skip auth in dev mode)
TOKEN="<bearer-token>"

# 4. Create an agent
curl -X POST http://localhost:3002/api/agents \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name":"test","role":"worker","system_prompt":"You are helpful.","provider":"mock","model":"mock"}'
# Expected: 201 Agent object with a UUID id

# 5. Run the agent
curl -X POST http://localhost:3002/api/agents/<id>/run \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"prompt":"Say hello."}'
# Expected: 200 with result string

# 6. Connect WebSocket and watch events
wscat -c ws://localhost:3002/events
# Then in another terminal, run the agent — you should see agent_started, agent_finished events
```

---

*Build `agent/` first, then `agent_server_cpp/`. The `mock` provider must work without any API key — use it for all local testing.*
