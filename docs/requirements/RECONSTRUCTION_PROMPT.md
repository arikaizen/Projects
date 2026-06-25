# Agent Studio — Project Reconstruction Prompt

You are building **Agent Studio** from scratch — a multi-model AI agent orchestration platform written in C++17. This prompt contains every detail needed to recreate the project exactly: directory layout, file names, function signatures, data models, API endpoints, protocols, build configuration, and architectural decisions. Follow every specification precisely.

---

## What You Are Building

Agent Studio lets users create autonomous AI agents, wire them together into pipelines, run them against tasks, and benchmark them across different AI providers. The backend is entirely C++. The frontend is a React + TypeScript web application that talks to the C++ backend over REST and WebSocket.

The project has six active components and two legacy Dart components that exist only to be deleted:

| Component | Language | Keep? |
|-----------|----------|-------|
| Agent Engine (`agent/`) | C++17 | Yes — core library |
| Agent Server (`agent_server_cpp/`) | C++17 | Yes — build this in Phase 1 |
| React Web UI (`ui/`) | TypeScript/React | Yes — build this in Phase 1 |
| MCP Tool Server (`mcp-server/`) | C++17 + Python | Yes — already exists |
| Auth Server (`auth-server/`) | C++17 | Yes — already exists |
| Data Server (`data server/`) | C++ | Yes — do not touch |
| Dart Agent Server (`agent_server/`) | Dart | Delete in Phase 2 |
| Dart MCP Server (`mcp_server/`) | Dart | Delete in Phase 2 |

---

## Project Root Layout

Create this exact directory structure:

```
Projects/
├── agent/                        # C++ Agent Engine — compiles to shared library
├── agent_server_cpp/             # C++ Agent Server — HTTP/WebSocket wrapper (build this)
├── ui/                           # React Web UI (TypeScript) — build this in Phase 1
├── agent_server/                 # Dart server — legacy, delete in Phase 2
├── mcp-server/                   # C++ MCP Tool Server — already exists
├── mcp_server/                   # Dart MCP server — legacy, delete in Phase 2
├── auth-server/                  # C++ Auth Server — already exists
├── data server/                  # C++ Data Server — do not touch
├── AI/                           # Standalone CLI project — separate concern
├── docs/
│   └── requirements/
│       ├── RECONSTRUCTION_PROMPT.md   # This file
│       ├── REQUIREMENTS.md
│       ├── 01-agent-engine.md
│       ├── 02-cpp-server.md
│       ├── 04-mcp-server.md
│       └── 05-decommission-dart.md
├── ARCHITECTURE_PLAN.md
├── docker-compose.yml
├── start.sh
├── deploy.sh
├── nginx.conf
└── README.md
```

---

## Component 1 — C++ Agent Engine (`agent/`)

### What it is

The agent engine is the heart of the entire platform. It is compiled as a shared library:
- Linux: `libagent_engine.so`
- macOS: `libagent_engine.dylib`

Everything that touches an AI model — calling it, routing between providers, running agents, piping output between agents, tracking shared state — happens exclusively inside this library. No other component is allowed to call an AI provider directly.

### Build System

File: `agent/CMakeLists.txt`
- CMake minimum version: 3.17
- C++ standard: 17
- Build flag: `-DAGENT_ENABLE_API_LLM=ON` enables cloud AI providers (Anthropic, OpenAI, etc.) and requires OpenSSL
- External dependencies: OpenSSL (when cloud enabled), nlohmann/json, third_party/ai_model

### Directory Structure

```
agent/
├── include/
│   ├── agent_engine/
│   │   └── c_api.h                  # The only public header — stable C ABI
│   └── agent/                       # Internal C++ headers
├── src/
│   ├── c_api/
│   │   └── c_api.cpp                # Implements every function declared in c_api.h
│   └── agent/
│       ├── agent_manager.cpp        # Core: lifecycle, state machine, event dispatch
│       ├── llm_factory.cpp          # Creates provider-specific AI clients from config
│       ├── ai_model_llm_client.cpp  # Adapter between AIModel hierarchy and engine
│       ├── event_bus.cpp            # Thread-safe event publish/subscribe
│       ├── blackboard.cpp           # Shared key-value store for all agents
│       ├── work_item.cpp            # A single unit of work passed through the pipeline
│       ├── quota.cpp                # Rate limit tracking per AI provider
│       ├── thread_pool.cpp          # Worker threads for async execution
│       ├── messaging.cpp            # Directed and broadcast messaging between agents
│       ├── actions/                 # One .cpp file per action type (14 total)
│       └── stages/                  # One .cpp file per pipeline stage (9 + 1 total)
├── third_party/
│   └── ai_model/                    # AIModel class hierarchy (adapters for each provider)
├── docs/
├── examples/
└── tests/
```

### The Public C API — `agent/include/agent_engine/c_api.h`

This is the only file external code uses. Declare every function below with C linkage (`extern "C"`).

Rules:
- All string parameters are null-terminated UTF-8 char pointers
- All strings returned by the engine are heap-allocated — the caller must free them with `am_free_string()`
- All JSON inputs and outputs use the schemas defined in the Data Models section of this document

```c
// Memory
void  am_free_string(char* str);

// Manager lifecycle
void* am_create();
void  am_destroy(void* handle);

// Agent CRUD
// config_json fields: name, role, system_prompt, provider, model, api_key, parent_id (nullable)
// Returns: heap-allocated agent ID string, or NULL on failure
char* am_spawn_agent(void* handle, const char* config_json);
void  am_destroy_agent(void* handle, const char* agent_id);
// Returns: JSON array — each element is an Agent object (see Data Models)
char* am_list_agents(void* handle);
// Returns: JSON Agent object
char* am_get_status(void* handle, const char* agent_id);

// Execution
// Returns: opaque future handle
void* am_run_agent(void* handle, const char* agent_id, const char* prompt);
// Blocks until the future resolves
// Returns: JSON — {result: string, duration_ms: int, prompt_tokens: int, completion_tokens: int, error: string|null}
char* am_future_wait(void* future_handle);
// override_json fields: prompt (optional), context (optional), priority (optional)
void  am_inject_work(void* handle, const char* agent_id, const char* override_json);
void  am_cancel_agent(void* handle, const char* agent_id);

// Wiring — connect output of one agent to the input of another
void  am_pipe(void* handle, const char* from_id, const char* to_id);
void  am_unpipe(void* handle, const char* from_id, const char* to_id);

// Messaging
// message_json fields: content, type, metadata (object)
void  am_send_message(void* handle, const char* from_id, const char* to_id, const char* message_json);
void  am_broadcast(void* handle, const char* from_id, const char* message_json);

// Shared blackboard (key-value store visible to all agents)
void  am_blackboard_write(void* handle, const char* key, const char* value_json);
// Returns: JSON value, or null string if key not found
char* am_blackboard_read(void* handle, const char* key);
// Returns: JSON array of key strings
char* am_blackboard_keys(void* handle);

// Events
// callback fires on engine worker threads — marshal to your own event loop before touching shared state
typedef void (*am_event_callback_t)(const char* event_json, void* user_data);
// Returns: subscription ID (pass to unsubscribe)
int   am_subscribe_events(void* handle, am_event_callback_t callback, void* user_data);
void  am_unsubscribe_events(void* handle, int subscription_id);

// MCP tool servers
void  am_connect_mcp(void* handle, const char* server_url, const char* auth_token);
void  am_disconnect_mcp(void* handle, const char* server_name);
// Returns: JSON array — each element: {name, url, tool_count, tools: []}
char* am_list_mcp_servers(void* handle);

// LLM configuration
// config_json fields: provider, model, api_key, base_url (optional)
void  am_configure_llm(void* handle, const char* config_json);
```

### Agent Execution Pipeline

Every time an agent processes a prompt, it runs through exactly 9 stages in this order. Implement each in `agent/src/agent/stages/<name>_stage.cpp`:

| Order | File | Stage name | What it does |
|-------|------|-----------|--------------|
| 1 | `observe_stage.cpp` | observe | Gathers all available context: messages received, pipe inputs, blackboard state, environment |
| 2 | `understand_stage.cpp` | understand | Parses and classifies the incoming work item — what kind of task is this? |
| 3 | `read_stage.cpp` | read | Pulls relevant data from conversation history and memory |
| 4 | `code_intel_stage.cpp` | code_intel | Analyzes any code artifacts found in context (symbols, file structure) |
| 5 | `locate_stage.cpp` | locate | Identifies which tools and MCP capabilities are relevant to this task |
| 6 | `inject_stage.cpp` | inject | Applies system prompt, role constraints, and any injection rules before calling the AI |
| 7 | `reason_stage.cpp` | reason | Calls the AI provider; parses the response into concrete action directives |
| 8 | `transform_stage.cpp` | transform | Post-processes and sanitizes AI output |
| 9 | `validate_stage.cpp` | validate | Checks output is correct; re-enters the loop if not, or proceeds to respond |
| — | `respond_stage.cpp` | respond | Emits the final output through the event bus; feeds any connected pipes |

### Action Types

During `reason_stage`, the AI response is parsed into action directives. Implement each action in `agent/src/agent/actions/<name>_action.cpp`:

| File | What it does |
|------|-------------|
| `bash_action.cpp` | Runs a shell command in a restricted subprocess; captures stdout and stderr |
| `read_action.cpp` | Reads file contents or looks up symbol definitions |
| `write_action.cpp` | Creates or overwrites a file |
| `edit_action.cpp` | Applies a targeted diff/edit to an existing file |
| `glob_action.cpp` | Returns all file paths matching a glob pattern |
| `grep_action.cpp` | Searches file contents by regular expression; returns matching lines with line numbers |
| `web_fetch_action.cpp` | Makes an HTTP GET or POST to an external URL; returns the response body |
| `web_search_action.cpp` | Submits a query to a configured search API; returns structured results |
| `mcp_tool_action.cpp` | Builds a JSON-RPC 2.0 request and sends it to the connected MCP tool server |
| `messaging_actions.cpp` | Calls `am_send_message` or `am_broadcast` |
| `blackboard_actions.cpp` | Calls `am_blackboard_read` or `am_blackboard_write` |
| `memory_actions.cpp` | Reads from or appends to conversation history |
| `task_action.cpp` | Spawns a sub-task or delegates to another agent via `am_run_agent` |
| `todo_write_action.cpp` | Writes a to-do item to disk for continuity across turns |

### AI Provider Support

Implement `makeLLMClientFromConfig(config_json)` in `agent/src/agent/llm_factory.cpp`. It reads the `"provider"` field and returns a provider-specific `AIModel` subclass. Support exactly these 16 providers:

| Provider value | Category | Notes |
|----------------|----------|-------|
| `anthropic` | Cloud | Messages API with streaming |
| `openai` | Cloud | Chat completions with streaming |
| `google` | Cloud | Gemini API |
| `groq` | Cloud | OpenAI-compatible endpoint |
| `mistral` | Cloud | Native Mistral API |
| `deepseek` | Cloud | OpenAI-compatible endpoint |
| `xai` | Cloud | Grok API |
| `openrouter` | Cloud aggregator | OpenAI-compatible; model names are provider-prefixed |
| `together` | Cloud aggregator | OpenAI-compatible |
| `ollama` | Local | REST at base_url; default `http://localhost:11434` |
| `llamacpp` | Local | REST at base_url |
| `lmstudio` | Local | OpenAI-compatible at base_url |
| `vllm` | Local | OpenAI-compatible at base_url |
| `llama` | Local | Direct llama.cpp integration via third_party/ai_model |
| `custom` | Any | OpenAI-compatible at user-supplied base_url |
| `mock` | Test | Returns deterministic canned responses; never touches the network |

### Event Bus

Implement in `agent/src/agent/event_bus.cpp`:
- Thread-safe pub/sub using a mutex and condition variable
- Callbacks fire on engine worker threads — callers are responsible for marshalling to their own event loop before modifying any shared state
- Each event is serialized to JSON and passed to every registered callback

The engine must emit exactly these event types:

| Event type string | When it fires | Payload fields |
|-------------------|--------------|---------------|
| `agent_started` | Agent begins execution | `agent_id`, `timestamp` |
| `work_item_started` | Agent begins processing a work item | `agent_id`, `work_item_id`, `prompt` |
| `agent_finished` | Agent completes successfully | `agent_id`, `result`, `duration_ms` |
| `agent_failed` | Agent hits an error it cannot recover from | `agent_id`, `error`, `code` |
| `agent_cancelled` | Agent was cancelled by the caller | `agent_id` |
| `blackboard_updated` | Any agent writes to the blackboard | `key`, `value` |
| `mcp_connected` | An MCP tool server was attached | `server_name`, `tool_count` |
| `mcp_disconnected` | An MCP tool server was removed | `server_name` |
| `quota_exceeded` | An agent hit a provider rate limit | `agent_id`, `provider`, `limit_type` |

Every event uses this JSON envelope:
```json
{
  "event": "<event_type_string>",
  "payload": { ... },
  "timestamp": "<ISO-8601 datetime string>"
}
```

---

## Component 2 — C++ Agent Server (`agent_server_cpp/`)

### What it is

A thin HTTP and WebSocket server that embeds `libagent_engine` and exposes its full C API over a network. This is the only server clients talk to. Build it in Phase 1.

### Build

File: `agent_server_cpp/CMakeLists.txt`
- C++ standard: 17
- Links: `libagent_engine`, cpp-httplib, nlohmann/json, OpenSSL
- Output binary: `agent_server_cpp/bin/agent_server`

### Authentication — Centralized Introspection

Every single inbound HTTP request must be authenticated. The server does this by calling the Auth Server:

```
POST <AUTH_SERVER_URL>/introspect
Body: {"token": "<bearer-token-from-Authorization-header>"}
```

If the response contains `"active": true` → accept the request.
If `"active": false` or the introspect call fails → return HTTP 401.

Cache valid introspection responses in memory for 60 seconds to avoid calling the Auth Server on every request.

### REST API

All endpoints require `Authorization: Bearer <token>`. Return `401` if invalid. Return `503` if the engine is unavailable.

---

**GET /health**
```json
{ "status": "ok", "version": "1.0.0", "engine_agents": 3 }
```

---

**GET /api/llm/models**

Query each configured provider's model-list API in real time and return the live results. Do not use a hardcoded list.

```json
[
  { "provider": "anthropic", "models": ["claude-opus-4-8", "claude-sonnet-4-6", "claude-haiku-4-5-20251001"] },
  { "provider": "openai", "models": ["gpt-4o", "gpt-4o-mini", "o1"] }
]
```

---

**POST /api/llm/configure**

Sets the default AI provider and model used by all agents that do not specify their own.

Request body:
```json
{ "provider": "anthropic", "model": "claude-sonnet-4-6", "api_key": "..." }
```
Response: `200 {"ok": true}`

Internally calls: `am_configure_llm(handle, config_json)`

---

**GET /api/agents**

Returns all registered agents.

Response: `200 [Agent, ...]`

Internally calls: `am_list_agents(handle)`

---

**POST /api/agents**

Creates and registers a new agent.

Request body:
```json
{
  "name": "research-agent",
  "role": "specialist",
  "system_prompt": "You are a research specialist...",
  "provider": "anthropic",
  "model": "claude-sonnet-4-6",
  "api_key": "...",
  "parent_id": null
}
```
Response: `201 Agent`

Internally calls: `am_spawn_agent(handle, config_json)`

---

**GET /api/agents/{id}**

Returns the current status of one agent.

Response: `200 Agent`

Internally calls: `am_get_status(handle, agent_id)`

---

**DELETE /api/agents/{id}**

Destroys an agent. If it is running, cancel it first.

Response: `204`

Internally calls: `am_cancel_agent` then `am_destroy_agent`

---

**POST /api/agents/{id}/run**

Executes an agent with a prompt. Blocks until complete. For streaming, the client uses the WebSocket `/events` channel simultaneously.

Request body:
```json
{ "prompt": "Research the latest advances in fusion energy." }
```
Response: `200 {"result": "...", "duration_ms": 4200, "prompt_tokens": 100, "completion_tokens": 350, "error": null}`

Internally calls: `am_run_agent` then `am_future_wait`

---

**POST /api/agents/{id}/inject**

Injects or overrides the agent's current work item while it is running.

Request body:
```json
{ "prompt": "...", "context": "...", "priority": 1 }
```
Response: `200 {"ok": true}`

Internally calls: `am_inject_work`

---

**POST /api/agents/{id}/cancel**

Cancels a running agent.

Response: `200 {"ok": true}`

Internally calls: `am_cancel_agent`

---

**POST /api/pipe**

Connects the output of one agent as the input of another agent.

Request body:
```json
{ "from_id": "agent-id-1", "to_id": "agent-id-2" }
```
Response: `200 {"ok": true}`

Internally calls: `am_pipe`

---

**DELETE /api/pipe**

Disconnects a pipe between two agents.

Request body:
```json
{ "from_id": "agent-id-1", "to_id": "agent-id-2" }
```
Response: `200 {"ok": true}`

Internally calls: `am_unpipe`

---

**POST /api/agents/{from_id}/send**

Sends a direct message from one agent to another.

Request body:
```json
{ "to_id": "agent-id-2", "content": "...", "type": "text", "metadata": {} }
```
Response: `200 {"ok": true}`

Internally calls: `am_send_message`

---

**POST /api/broadcast**

Broadcasts a message from one agent to all other agents.

Request body:
```json
{ "from_id": "agent-id-1", "content": "..." }
```
Response: `200 {"ok": true}`

Internally calls: `am_broadcast`

---

**GET /api/blackboard**

Lists all keys currently in the shared blackboard.

Response: `200 {"keys": ["key1", "key2", ...]}`

Internally calls: `am_blackboard_keys`

---

**GET /api/blackboard/{key}**

Reads one value from the blackboard.

Response: `200 {"key": "...", "value": <any JSON value>}`

Internally calls: `am_blackboard_read`

---

**POST /api/blackboard/{key}**

Writes a value to the blackboard.

Request body:
```json
{ "value": <any JSON value> }
```
Response: `200 {"ok": true}`

Internally calls: `am_blackboard_write`

---

**GET /api/mcp**

Lists all MCP tool servers currently connected to the engine.

Response: `200 [{"name": "...", "url": "...", "tool_count": 50, "tools": [...]}]`

Internally calls: `am_list_mcp_servers`

---

**POST /api/mcp**

Connects a new MCP tool server to the engine.

Request body:
```json
{ "url": "http://localhost:8081", "auth_token": "..." }
```
Response: `200 {"ok": true}`

Internally calls: `am_connect_mcp`

---

**DELETE /api/mcp/{name}**

Disconnects a named MCP tool server.

Response: `200 {"ok": true}`

Internally calls: `am_disconnect_mcp`

---

**POST /api/benchmark**

Runs a benchmark: sends the same prompt to multiple agents simultaneously, collects metrics, and runs LLM-as-judge quality scoring.

Request body:
```json
{
  "agent_ids": ["agent-id-1", "agent-id-2"],
  "prompt": "Explain quantum entanglement in simple terms."
}
```

Implementation steps:
1. Run all listed agents concurrently with the same prompt using `am_run_agent` + `am_future_wait`
2. Record `latency_ms`, `ttft_ms` (time to first token), `tokens_per_sec`, `prompt_tokens`, `completion_tokens`, `total_tokens`, `est_cost_usd` from timing and token counts
3. For each result, call the judge AI model (configured via `JUDGE_MODEL` and `JUDGE_PROVIDER` env vars) with this prompt:

```
You are an impartial evaluator. Rate the quality of the following AI response on a scale from 0 to 10, where 0 is completely wrong or unhelpful and 10 is perfect.

Original question: <prompt>
AI response: <result>

Respond with only a JSON object: {"score": <number>}
```

4. Parse the score from the judge response
5. Return an array of BenchmarkResult objects (see Data Models)

Response: `200 [BenchmarkResult, ...]`

---

**WebSocket /events**

Persistent two-way WebSocket connection. The server streams all engine events to every connected client.

Implementation:
- On client connect: call `am_subscribe_events` and register a callback
- The callback fires on engine worker threads — it must only write the event JSON string into a thread-safe queue (mutex-protected `std::queue<std::string>`)
- A dedicated sender thread runs in a loop: drains the queue and calls `ws_send()` to all connected clients
- On client disconnect: call `am_unsubscribe_events` and remove the client from the sender list
- Optional: clients may send `{"filter": {"agent_id": "..."}}` after connecting to receive only events for a specific agent

Every message sent over the socket uses this format:
```json
{
  "event": "<event_type>",
  "payload": { ... },
  "timestamp": "<ISO-8601>"
}
```

---

## Component 3 — C++ MCP Tool Server (`mcp-server/`)

### What it is

A JSON-RPC 2.0 HTTP server that exposes 50+ external tools to the Agent Engine. The C++ layer handles HTTP, authentication, and dispatching. Each individual tool is a Python 3 script — this makes adding new tools trivially easy.

### Build

File: `mcp-server/CMakeLists.txt`
- C++ standard: 17
- Libraries: cpp-httplib, nlohmann/json, OpenSSL
- Output: `mcp-server/bin/mcp_server`
- Default port: **8081** (override with `MCP_PORT` env var)

### Directory Structure

```
mcp-server/
├── src/
│   ├── main.cpp              # HTTP server setup and route registration
│   ├── tool_registry.cpp     # Loads tools_manifest.json; serves the tool list
│   ├── tool_runner.cpp       # Spawns Python subprocesses; pipes JSON in/out
│   └── auth.cpp              # Bearer token validation via Auth Server introspect
├── tools/
│   ├── tools_manifest.json   # Registry of all tools with schemas
│   ├── _base.py              # Shared Python helpers for all tool scripts
│   └── <category>/
│       └── <tool_name>.py    # One file per tool
├── CMakeLists.txt
├── .env.example
└── docs/
    └── README.md
```

### HTTP Endpoints

All require `Authorization: Bearer <token>`.

**GET /tools** — Returns the full tool manifest (array of tool descriptors)

**POST /rpc** — Executes a tool using JSON-RPC 2.0

Request:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tool/call",
  "params": {
    "name": "web_search",
    "arguments": { "query": "..." }
  }
}
```

Success response:
```json
{ "jsonrpc": "2.0", "id": 1, "result": { "output": "..." } }
```

Error response:
```json
{ "jsonrpc": "2.0", "id": 1, "error": { "code": -32000, "message": "..." } }
```

### How Tool Execution Works (`tool_runner.cpp`)

1. Look up `name` in the tool registry to find the Python script path
2. Spawn: `python3 tools/<category>/<tool>.py`
3. Write the `arguments` JSON object to the process's **stdin**
4. Read the result JSON object from **stdout**
5. Read any errors from **stderr** (log them; do not forward to client)
6. **Never** put API credentials in the JSON — inject them as **environment variables** when spawning the subprocess

### Authentication (`auth.cpp`)

- Extract Bearer token from `Authorization: Bearer <token>` header
- Call `POST <AUTH_SERVER_URL>/introspect` with body `{"token": "<token>"}`
- Cache valid token responses in an in-memory map for 60 seconds (use a mutex)
- If `"active": false` or the call fails → return HTTP 401

### tools_manifest.json — Tool Registry Format

Each entry:
```json
{
  "name": "web_search",
  "category": "search",
  "description": "Search the web and return top results",
  "script": "search/web_search.py",
  "input_schema": {
    "type": "object",
    "properties": {
      "query": { "type": "string", "description": "The search query" },
      "num_results": { "type": "integer", "default": 5 }
    },
    "required": ["query"]
  }
}
```

### Tool Categories (implement all 50+)

| Category | Tools to implement |
|----------|--------------------|
| search | web_search, news_search, academic_search |
| finance | stock_price, crypto_price, exchange_rate |
| weather | current_weather, forecast, historical_weather |
| code | code_execute, code_lint, code_format |
| data | json_parse, csv_parse, xml_parse |
| communication | email_send, slack_message, sms_send |
| calendar | calendar_list, calendar_create, calendar_delete |
| files | file_read, file_write, file_list |
| utilities | url_fetch, qr_generate, hash_compute |
| ai | image_describe, text_translate, text_summarize |

### `_base.py` — Shared Helper

```python
import json, sys

def read_input():
    return json.loads(sys.stdin.read())

def write_output(data):
    print(json.dumps(data))
    sys.stdout.flush()

def write_error(msg):
    print(json.dumps({"error": msg}))
    sys.stdout.flush()
```

Every tool script follows this pattern:
```python
from _base import read_input, write_output, write_error
import os

def run(args):
    api_key = os.environ.get("SOME_API_KEY")
    # implement the tool logic here
    return {"output": "..."}

if __name__ == "__main__":
    args = read_input()
    write_output(run(args))
```

---

## Component 4 — C++ Auth Server (`auth-server/`)

### What it is

An OAuth 2.1 authorization server with PKCE (Proof Key for Code Exchange) and RS256 JWT (JSON Web Token). It issues access tokens used by all other services.

### Build

File: `auth-server/CMakeLists.txt`
- C++ standard: 17
- Libraries: cpp-httplib, OpenSSL
- Output: `auth-server/bin/auth_server`
- Default port: **8080**

### Directory Structure

```
auth-server/
├── src/
│   ├── main.cpp        # HTTP server setup and route registration
│   ├── auth.cpp        # PKCE flow, JWT signing/verification, refresh token logic
│   └── store.cpp       # In-memory state: clients, auth codes, tokens, refresh tokens
├── CMakeLists.txt
├── .env.example
└── docs/
    └── README.md
```

### In-Memory State (`store.cpp`)

```cpp
struct Client {
    string id, secret, redirect_uri;
};

struct AuthCode {
    string code, client_id, sub, role, code_challenge;
    time_t expires;
};

struct Token {
    string jti, sub, role;
    time_t expires;
    bool revoked;
};

struct RefreshToken {
    string token, sub, client_id, role;
    time_t expires;
    bool used;
};
```

### Endpoints

**POST /register** — Dynamic client registration. Returns `client_id` and `client_secret`.

**GET /authorize** — Authorization endpoint. Accepts: `client_id`, `redirect_uri`, `code_challenge`, `code_challenge_method=S256`. Returns an authorization code.

**POST /token** — Token exchange.
- `grant_type=authorization_code`: accepts `code`, `code_verifier`, `client_id` → returns access token + refresh token
- `grant_type=refresh_token`: accepts `refresh_token` → returns new access token

**POST /introspect** — Token validation for resource servers (Agent Server, MCP Server).
- Request body: `{"token": "<token>"}`
- Response if valid: `{"active": true, "sub": "...", "role": "...", "exp": 1234567890}`
- Response if invalid or expired: `{"active": false}`

**GET /.well-known/oauth-authorization-server** — Server metadata in standard OAuth format.

**GET /.well-known/jwks.json** — Public key set in JWK format for RS256 signature verification.

### Token Properties

| Property | Value |
|----------|-------|
| Signing algorithm | RS256 |
| Access token lifetime | 3600 seconds (1 hour), configurable via `TOKEN_TTL` env var |
| Refresh token lifetime | 2592000 seconds (30 days), configurable via `REFRESH_TTL` env var |
| JWT claims | `sub`, `aud`, `iat`, `exp`, `role`, `scope`, `jti` |

### Environment Variables

```bash
JWT_PRIVATE_KEY_PATH=./keys/private.pem
JWT_PUBLIC_KEY_PATH=./keys/public.pem
AUTH_PORT=8080
TOKEN_TTL=3600
REFRESH_TTL=2592000
```

---

## Component 5 — C++ Data Server (`data server/`)

Do not modify this component. It is a graph-based persistence backend for agent relationship data. It uses a custom protocol and is not exposed to any external clients yet.

```
data server/
├── main.cpp
├── server/server.cpp
├── graph_store/graph_store.cpp
├── persistence/persistence.cpp
└── Makefile
```

---

## Component 6 — React Web UI (`ui/`)

### What it is

A browser-based single-page application that is the only client of the C++ Agent Server. It communicates exclusively over REST and WebSocket — no direct connection to the engine. All business logic stays in the C++ backend; the UI only displays state and sends commands.

### Tech Stack

| Layer | Library | Purpose |
|-------|---------|---------|
| Framework | React 18 + TypeScript | Component model, strict typing |
| Build | Vite | Fast dev server, production bundler |
| Styling | Tailwind CSS | Utility-first styling |
| Components | shadcn/ui | Accessible, composable base components |
| Data fetching | TanStack Query | REST calls, caching, loading states, auto-refetch |
| WebSocket | reconnecting-websocket | Persistent connection to `/events` with auto-reconnect |
| Charts | Recharts | Benchmark leaderboard and metric charts |
| Drag and drop | @dnd-kit/core | Pipeline canvas (Phase 5) |
| Routing | React Router v6 | Multi-page navigation |

### Directory Structure

```
ui/
├── src/
│   ├── api/
│   │   ├── agents.ts          # REST calls: CRUD agents, run, cancel, inject
│   │   ├── groups.ts          # REST calls: CRUD groups, run
│   │   ├── benchmark.ts       # POST /api/benchmark
│   │   ├── blackboard.ts      # GET/POST /api/blackboard
│   │   ├── mcp.ts             # GET/POST/DELETE /api/mcp
│   │   └── models.ts          # GET /api/llm/models
│   ├── hooks/
│   │   ├── useAgents.ts       # TanStack Query wrappers for agent endpoints
│   │   ├── useGroups.ts       # TanStack Query wrappers for group endpoints
│   │   ├── useBenchmark.ts    # Benchmark runner hook
│   │   └── useEvents.ts       # WebSocket /events hook
│   ├── types/
│   │   ├── agent.ts           # Agent, AgentStatus, AgentRole types
│   │   ├── group.ts           # AgentGroup, GroupMode types
│   │   ├── task.ts            # Task type
│   │   └── benchmark.ts       # BenchmarkResult type
│   ├── components/
│   │   ├── AgentCard.tsx      # Compact tile: name, role, model, status, actions
│   │   ├── ChatPanel.tsx      # Conversation panel with streaming markdown output
│   │   ├── StatusBadge.tsx    # Animated badge: idle/running/waiting/done/error
│   │   ├── HierarchyTree.tsx  # Visual directed graph of agent relationships
│   │   ├── BenchmarkTable.tsx # Sortable leaderboard with metric columns
│   │   ├── GroupCard.tsx      # Group tile with mode label and member count
│   │   ├── LogFeed.tsx        # Live scrolling event log
│   │   └── ModelSelector.tsx  # Provider + model dropdown fed from /api/llm/models
│   ├── pages/
│   │   ├── Dashboard.tsx      # Root layout with six-tab navigation
│   │   ├── Agents.tsx         # Agent list + create/edit drawer
│   │   ├── Groups.tsx         # Group list + create/edit drawer
│   │   ├── Hierarchy.tsx      # Agent relationship canvas
│   │   ├── Benchmark.tsx      # Benchmark runner + leaderboard
│   │   ├── Tasks.tsx          # Task history table
│   │   ├── Logs.tsx           # Live engine event feed
│   │   └── Settings.tsx       # MCP servers, default LLM, API keys
│   ├── store/
│   │   └── eventStore.ts      # In-memory store updated by WebSocket events
│   ├── auth/
│   │   ├── AuthContext.tsx    # Token storage, silent refresh, role
│   │   └── LoginPage.tsx      # OAuth 2.1 + PKCE login flow
│   └── main.tsx
├── index.html
├── vite.config.ts
├── tailwind.config.ts
├── tsconfig.json
└── package.json
```

### Pages

**Dashboard** — six-tab layout:

| Tab | Content |
|-----|---------|
| Agents | Agent list with status badges; create/edit/delete; run with prompt |
| Groups | Group list; create/edit; run with prompt |
| Hierarchy | Visual directed graph of pipe and parent/child relationships |
| Benchmark | Run same prompt across multiple agents; sortable leaderboard |
| Tasks | Task history: prompt, target, status, duration, result |
| Logs | Live scrolling engine event feed with timestamps and event types |

**Settings** — MCP server URL list (add/remove), default LLM provider and model (populated from `GET /api/llm/models`), per-provider API key fields.

**Agent create/edit drawer** — 4-step form:
1. Name and role (orchestrator / worker / specialist / reviewer / planner)
2. System prompt text editor
3. Provider and model selection (dynamic list from server)
4. Review and save

**Group create/edit drawer** — Name, collaboration mode (5 modes), member agent multi-select, edge mapping for sequential and pipeline modes.

### WebSocket Event Hook

```typescript
// hooks/useEvents.ts
import ReconnectingWebSocket from 'reconnecting-websocket';
import { useEffect } from 'react';

export function useEvents(onEvent: (event: AgentEvent) => void) {
  useEffect(() => {
    const ws = new ReconnectingWebSocket(`${import.meta.env.VITE_WS_URL}/events`, [], {
      maxRetries: Infinity,
      reconnectionDelayGrowFactor: 2,
      minReconnectionDelay: 2000,
      maxReconnectionDelay: 16000,
    });
    ws.onmessage = (msg) => onEvent(JSON.parse(msg.data));
    return () => ws.close();
  }, []);
}
```

Events update `eventStore.ts`. TanStack Query invalidates relevant caches on `agent_finished` and `agent_failed` so lists re-fetch automatically.

### REST API Integration

All calls use TanStack Query. Base URL set via `VITE_API_URL` env var. Every request attaches the Bearer token from `AuthContext`.

```typescript
// api/agents.ts
const API = import.meta.env.VITE_API_URL;

export const getAgents = () =>
  fetch(`${API}/api/agents`, { headers: authHeaders() }).then(r => r.json());

export const createAgent = (config: AgentConfig) =>
  fetch(`${API}/api/agents`, {
    method: 'POST',
    headers: { ...authHeaders(), 'Content-Type': 'application/json' },
    body: JSON.stringify(config),
  }).then(r => r.json());
```

### Authentication Flow (OAuth 2.1 + PKCE)

1. User clicks login → UI generates code verifier and code challenge
2. Redirect to `GET <AUTH_SERVER>/authorize` with `code_challenge` and `code_challenge_method=S256`
3. Auth Server returns an authorization code
4. UI exchanges code for access + refresh token via `POST <AUTH_SERVER>/token`
5. Access token stored in memory only (not localStorage); refresh token in HTTP-only cookie
6. Every API request includes `Authorization: Bearer <token>`
7. On 401 → silently refresh token and retry the request once

### TypeScript Types

Define these types in `ui/src/types/` to match the data models exactly:

```typescript
// agent.ts
type AgentStatus = 'idle' | 'running' | 'waiting' | 'done' | 'error' | 'cancelled';
type AgentRole = 'orchestrator' | 'worker' | 'specialist' | 'reviewer' | 'planner';

interface Agent {
  id: string;
  name: string;
  user_id: string;
  role: AgentRole;
  system_prompt: string;
  model: string;
  provider: string;
  status: AgentStatus;
  parent_id: string | null;
  metadata: Record<string, unknown>;
}

// group.ts
type GroupMode = 'parallel' | 'sequential' | 'broadcast' | 'consensus' | 'pipeline';

interface AgentGroup {
  id: string;
  name: string;
  mode: GroupMode;
  agents: string[];
  edges: Record<string, string>;
}

// benchmark.ts
interface BenchmarkResult {
  id: string;
  agent_id: string;
  agent_name: string;
  model: string;
  provider: string;
  prompt: string;
  speed: { latency_ms: number; ttft_ms: number; tokens_per_sec: number };
  cost: { prompt_tokens: number; completion_tokens: number; total_tokens: number; est_cost_usd: number };
  quality: { score: number; method: string };
  reliability: { success: boolean; retries: number; error: string | null };
  timestamp: string;
}
```

### Environment Variables (`ui/.env`)

```bash
VITE_API_URL=http://localhost:3002      # C++ Agent Server base URL
VITE_WS_URL=ws://localhost:3002         # C++ Agent Server WebSocket URL
VITE_AUTH_URL=http://localhost:8080     # C++ Auth Server base URL
```

### Build and Run

```bash
# Development
cd ui && npm install && npm run dev     # Vite dev server at http://localhost:5173

# Production
cd ui && npm run build                  # Static files output to ui/dist/
# nginx serves ui/dist/ and proxies /api/, /events, /auth/ to backend services
```

---

## Component 7 — Dart Agent Server (`agent_server/`) — DELETE IN PHASE 2

This server re-implements agent execution and AI routing independently of the C++ engine, creating dangerous duplication. It runs on port 3001. Do not add any new features. Delete the entire directory in Phase 2.

Legacy endpoints it exposes (for reference only):
```
GET/POST        /api/agents
GET/PUT/DELETE  /api/agents/{id}
POST            /api/agents/{id}/run     (SSE stream)
GET/POST        /api/groups
GET/PUT/DELETE  /api/groups/{id}
POST            /api/groups/{id}/run     (SSE stream)
```

---

## Data Models

Every component that sends or receives JSON must use exactly these schemas.

### Agent

```json
{
  "id": "uuid-v4",
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

`status` must be one of: `idle`, `running`, `waiting`, `done`, `error`, `cancelled`
`role` must be one of: `orchestrator`, `worker`, `specialist`, `reviewer`, `planner`

### AgentGroup

```json
{
  "id": "uuid-v4",
  "name": "research-team",
  "mode": "sequential",
  "agents": ["agent-id-1", "agent-id-2"],
  "edges": {
    "agent-id-1": "agent-id-2"
  }
}
```

`mode` must be one of: `parallel`, `sequential`, `broadcast`, `consensus`, `pipeline`

`edges` maps `from_agent_id` → `to_agent_id`. Used only in `sequential` and `pipeline` modes.

### Group Collaboration Modes (how they work)

| Mode | Behaviour |
|------|-----------|
| `parallel` | All agents receive the same prompt at the same time; results are collected independently |
| `sequential` | Agents execute one after another; each agent receives the previous agent's output as its input |
| `broadcast` | A designated lead agent sends a message; all others respond independently |
| `consensus` | All agents produce a response; a judge agent (role: `reviewer`) selects or merges the best answer |
| `pipeline` | Agents are wired via `am_pipe()`; data flows along edges defined in the group's `edges` map |

### Task

```json
{
  "id": "uuid-v4",
  "prompt": "Research the impact of climate change on agriculture.",
  "target_id": "agent-id-1",
  "target_type": "agent",
  "status": "done",
  "started_at": "2026-06-25T10:00:00Z",
  "finished_at": "2026-06-25T10:00:45Z",
  "duration_ms": 45000,
  "result": "..."
}
```

`target_type` is either `"agent"` or `"group"`

### BenchmarkResult

```json
{
  "id": "uuid-v4",
  "agent_id": "agent-id-1",
  "agent_name": "GPT-4o Agent",
  "model": "gpt-4o",
  "provider": "openai",
  "prompt": "Explain quantum entanglement in simple terms.",
  "speed": {
    "latency_ms": 2500,
    "ttft_ms": 150,
    "tokens_per_sec": 35.2
  },
  "cost": {
    "prompt_tokens": 100,
    "completion_tokens": 250,
    "total_tokens": 350,
    "est_cost_usd": 0.00525
  },
  "quality": {
    "score": 8.7,
    "method": "llm_judge"
  },
  "reliability": {
    "success": true,
    "retries": 0,
    "error": null
  },
  "timestamp": "2026-06-25T10:05:00Z"
}
```

---

## Deployment Configuration

### `start.sh` — One-Command Stack Launcher

```bash
#!/usr/bin/env bash
# ./start.sh              — build everything + start all services + public tunnel
# ./start.sh --no-build  — skip compilation, just start
# ./start.sh --no-tunnel — no public tunnel (LAN only)
# ./start.sh --stop      — stop all running services
```

Build steps in order:
1. `cmake --build agent/build` — compile `libagent_engine`
2. `cmake --build agent_server_cpp/build` — compile the Agent Server
3. `cmake --build mcp-server/build` — compile the MCP Tool Server
4. `cmake --build auth-server/build` — compile the Auth Server

Services to start (track PIDs in `.run/*.pid`, redirect output to `.run/<name>.log`):
- Auth Server → port 8080
- MCP Tool Server → port 8081
- Agent Server → port defined by `AGENT_SERVER_PORT`
- Cloudflare tunnel → public HTTPS URL pointing to the Agent Server

Required helper functions:
- `free_port <port>` — kill any process currently bound to that port
- `start_svc <name> <cmd...>` — launch with `nohup`, save PID, redirect logs
- `stop_all` — read all `.run/*.pid` files, send SIGTERM, clean up files
- `ensure_cloudflared` — check if `cloudflared` is on PATH or in `.run/`; download if missing

### `docker-compose.yml`

Services:
- `auth_server` — builds from `auth-server/Dockerfile`; exposes port 8080
- `mcp_server` — builds from `mcp-server/Dockerfile`; exposes port 8081
- `agent_server` — builds from `agent_server_cpp/Dockerfile`; exposes Agent Server port
- `nginx` — reverse proxy using `nginx.conf`

### `nginx.conf` — Reverse Proxy Rules

- `/api/` and `/events` → forward to Agent Server (upgrade `/events` to WebSocket)
- `/auth/` → forward to Auth Server
- `/tools/` and `/rpc` → forward to MCP Tool Server

### Root `.env.example`

```bash
# Auth Server
JWT_PRIVATE_KEY_PATH=./auth-server/keys/private.pem
JWT_PUBLIC_KEY_PATH=./auth-server/keys/public.pem
AUTH_PORT=8080
TOKEN_TTL=3600
REFRESH_TTL=2592000

# MCP Tool Server
MCP_PORT=8081
AUTH_SERVER_URL=http://localhost:8080
MCP_AUTH_TOKEN=<shared-bearer-token>

# Agent Server
AGENT_SERVER_PORT=3002
AUTH_SERVER_URL=http://localhost:8080

# LLM-as-judge (used by benchmark endpoint)
JUDGE_PROVIDER=anthropic
JUDGE_MODEL=claude-sonnet-4-6
JUDGE_API_KEY=<key>

# Tool API Keys (MCP server injects these into Python subprocesses)
OPENAI_API_KEY=
ANTHROPIC_API_KEY=
GOOGLE_API_KEY=
SERP_API_KEY=
WEATHER_API_KEY=
ALPHA_VANTAGE_KEY=
SENDGRID_API_KEY=
SLACK_BOT_TOKEN=
TWILIO_ACCOUNT_SID=
TWILIO_AUTH_TOKEN=
```

---

## Performance and Safety Constraints

These are hard requirements — not suggestions. Violating them is a bug.

| Constraint | Required value |
|------------|---------------|
| Agent spawn time | Under 100 ms |
| Time to first AI token forwarded to client | Under 500 ms (excluding AI provider network latency) |
| Concurrent agents without degradation | At least 20 |
| WebSocket event delivery latency | Under 200 ms |
| AI call retry policy | Up to 3 retries, exponential backoff: 1 s, 2 s, 4 s |
| Token validation cache TTL | 60 seconds |
| LLM API keys | Never logged, never returned in API responses, never embedded in tool JSON |
| C API (`c_api.h`) ABI | Frozen — breaking changes require a major version bump |
| Adding a new AI provider | Only `llm_factory.cpp` changes |
| Adding a new MCP tool | One Python script + one manifest entry only |
| Bash action sandbox | Restricted filesystem; no outbound network by default |
| TLS in production | Enforced on all inter-service HTTP connections |

---

## Build Order

Build and verify in this order:

**Backend (C++)**
1. `cmake -B agent/build -DAGENT_ENABLE_API_LLM=ON && cmake --build agent/build`
2. `cmake -B auth-server/build && cmake --build auth-server/build`
3. `cmake -B mcp-server/build && cmake --build mcp-server/build`
4. `cmake -B agent_server_cpp/build && cmake --build agent_server_cpp/build`

**Frontend (React)**
5. `cd ui && npm install && npm run dev`

Verify backends:
- `curl http://localhost:8080/.well-known/oauth-authorization-server` → Auth Server metadata
- `curl -H "Authorization: Bearer <token>" http://localhost:8081/tools` → MCP tool list
- `curl -H "Authorization: Bearer <token>" http://localhost:3002/health` → Agent Server health

Verify frontend:
- Open `http://localhost:5173` in a browser → login page loads
- After login, Dashboard loads with Agents tab
- Create an agent → it appears in the list
- Run the agent → status badge updates in real time via WebSocket

---

## Migration Phases — Implement In This Order

| Phase | What to do |
|-------|-----------|
| **1** | Build the C++ Agent Server (`agent_server_cpp/`) and the React UI (`ui/`). The Agent Server must expose all REST endpoints, WebSocket `/events`, `GET /api/llm/models`, LLM-as-judge benchmarking, and centralized auth introspection. The React UI must connect to it and display agents, groups, benchmark results, and the live event log. |
| **2** | Delete `agent_server/` (Dart) and `mcp_server/` (Dart) directories entirely. |
| **3** | Update `start.sh` and `docker-compose.yml` to remove all Dart references; add React build step and static file serving via nginx. |
| **4** | Verify the full stack end-to-end: login in the browser, create an agent, run it, watch the event stream update the UI in real time, run a benchmark and confirm LLM-as-judge scores appear in the leaderboard. |
| **5** | Add drag-and-drop pipeline canvas to the Hierarchy page using @dnd-kit/core. |

---

*End of reconstruction prompt. Every directory, file, function, endpoint, data model, and constraint above must be implemented exactly as specified.*
