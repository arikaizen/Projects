# Agent Studio — Software Requirements Document

**Version**: 1.0  
**Date**: 2026-06-25  
**Status**: Active  

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [System Architecture](#2-system-architecture)
3. [Component Descriptions](#3-component-descriptions)
   - 3.1 [C++ Agent Engine](#31-c-agent-engine)
   - 3.2 [Flutter GUI — Agent Studio](#32-flutter-gui--agent-studio)
   - 3.3 [C++ Agent Server](#33-c-agent-server)
   - 3.4 [C++ MCP Tool Server](#34-c-mcp-tool-server)
   - 3.5 [C++ Auth Server](#35-c-auth-server)
   - 3.6 [C++ Data Server](#36-c-data-server)
   - 3.7 [Dart Agent Server (Decommission Target)](#37-dart-agent-server-decommission-target)
4. [Functional Requirements](#4-functional-requirements)
5. [Non-Functional Requirements](#5-non-functional-requirements)
6. [Data Models](#6-data-models)
7. [API Specification](#7-api-specification)
8. [Event System](#8-event-system)
9. [Integration Requirements](#9-integration-requirements)
10. [Deployment Requirements](#10-deployment-requirements)
11. [Migration Phases](#11-migration-phases)
12. [Open Decisions](#12-open-decisions)

---

## 1. Project Overview

**Agent Studio** is a multi-model AI agent orchestration platform. It provides a graphical desktop and web interface for creating, configuring, running, and benchmarking autonomous AI agents. Agents can operate individually, be wired together in directed pipelines, or collaborate in groups using one of five coordination modes.

### Goals

- Provide a single unified platform for managing agents across multiple LLM providers.
- Consolidate all LLM inference calls into the C++ agent engine — no direct LLM calls in the GUI or intermediate servers.
- Support both desktop (FFI) and web (HTTP/WebSocket) deployments from a single Flutter codebase.
- Enable real-time visibility into agent execution through a streaming event system.
- Offer quantitative benchmarking of agents across speed, cost, quality, and reliability dimensions.

### Stakeholders

| Role | Concern |
|------|---------|
| End User (Admin) | Create and manage agents, run tasks, compare models |
| End User (Standard) | Run assigned agents, view results |
| Developer | Extend the engine, add MCP tools, maintain services |
| Operator | Deploy, monitor, and scale the platform |

---

## 2. System Architecture

### High-Level Topology

```
┌──────────────────────────────────────────────────────────┐
│                  Flutter GUI (agent_studio)               │
│   AgentProvider ← AuthProvider ← CloudProvider           │
└───────────────────────┬──────────────────────────────────┘
                        │  AgentBackend (abstract interface)
          ┌─────────────┼─────────────┐
          │             │             │
     FfiBackend    HttpBackend   MockBackend
     (desktop)      (web)        (offline)
          │             │
          │         REST + WebSocket
          │             │
    libagent_engine.so  │
          │             ▼
          └──── C++ Agent Server (Phase 1 target)
                        │  embeds
                        ▼
              ┌─────────────────────┐
              │   C++ Agent Engine  │
              │   (single LLM hub)  │
              └──────────┬──────────┘
                         │ am_connect_mcp
                         ▼
              ┌─────────────────────┐
              │  C++ MCP Tool Server│
              │  (50+ tools)        │
              └─────────────────────┘
                         │
              ┌─────────────────────┐
              │  C++ Auth Server    │
              │  OAuth 2.1 + PKCE   │
              └─────────────────────┘
                         │
              LLM Providers / External APIs
```

### Backend Services and Ports

| Service | Language | Port | Status |
|---------|----------|------|--------|
| Agent Engine | C++17 | — (library) | Active |
| C++ Agent Server | C++17 | TBD | Planned (Phase 1) |
| C++ MCP Tool Server | C++17 | 8081 | Active |
| C++ Auth Server | C++17 | 8080 | Active |
| C++ Data Server | C++ | custom | Active (out of scope) |
| Flutter Web GUI | Dart/Flutter | 8080 | Active |
| Dart Agent Server | Dart | 3001 | Decommission target |
| Dart MCP Server | Dart | 3000 | Decommission target |

---

## 3. Component Descriptions

### 3.1 C++ Agent Engine

**Location**: `agent/`  
**Language**: C++17  
**Build**: CMake 3.17+, optional `-DAGENT_ENABLE_API_LLM=ON` for cloud LLM support  

The agent engine is the core runtime of the platform. It is compiled as a shared library (`libagent_engine.so`) and exposed via a stable C ABI (`agent/include/agent_engine/c_api.h`). All LLM inference, agent lifecycle, tool execution, inter-agent communication, and event emission live here.

#### 3.1.1 C Public API

The engine exposes the following C functions grouped by concern:

**Lifecycle**
| Function | Description |
|----------|-------------|
| `am_create()` | Instantiate a new AgentManager |
| `am_destroy(handle)` | Shut down and free an AgentManager |

**Agent CRUD**
| Function | Description |
|----------|-------------|
| `am_spawn_agent(handle, config_json)` | Create and register a new agent from JSON config; returns agent ID |
| `am_destroy_agent(handle, agent_id)` | Stop and unregister an agent |
| `am_list_agents(handle)` | Return JSON array of all registered agents with status |
| `am_get_status(handle, agent_id)` | Return current status and metadata for one agent |

**Execution**
| Function | Description |
|----------|-------------|
| `am_run_agent(handle, agent_id, prompt)` | Start agent execution; returns a future handle |
| `am_future_wait(future_handle)` | Block until the future resolves; returns result JSON |
| `am_inject_work(handle, agent_id, override_json)` | Inject or override the agent's current work item mid-run |
| `am_cancel_agent(handle, agent_id)` | Request graceful cancellation of a running agent |

**Wiring**
| Function | Description |
|----------|-------------|
| `am_pipe(handle, from_id, to_id)` | Connect output of `from` agent as input to `to` agent |
| `am_unpipe(handle, from_id, to_id)` | Disconnect a previously established pipe |

**Messaging**
| Function | Description |
|----------|-------------|
| `am_send_message(handle, from_id, to_id, message_json)` | Send a directed message between agents |
| `am_broadcast(handle, from_id, message_json)` | Broadcast a message to all agents |

**Shared State (Blackboard)**
| Function | Description |
|----------|-------------|
| `am_blackboard_write(handle, key, value_json)` | Write a value to the shared blackboard |
| `am_blackboard_read(handle, key)` | Read a value from the shared blackboard |
| `am_blackboard_keys(handle)` | List all keys currently in the blackboard |

**Events**
| Function | Description |
|----------|-------------|
| `am_subscribe_events(handle, callback_fn, user_data)` | Register a callback for all engine events |
| `am_unsubscribe_events(handle, subscription_id)` | Remove a registered event callback |

**MCP Integration**
| Function | Description |
|----------|-------------|
| `am_connect_mcp(handle, server_url, auth_token)` | Attach an MCP tool server to the engine |
| `am_disconnect_mcp(handle, server_name)` | Detach a named MCP server |
| `am_list_mcp_servers(handle)` | Return JSON array of connected MCP servers and their tools |

**LLM Configuration**
| Function | Description |
|----------|-------------|
| `am_configure_llm(handle, config_json)` | Set the default LLM provider, model, and API key at runtime |

#### 3.1.2 Agent Execution Pipeline (OODA Loop)

Each agent turn passes through 9 sequential stages:

| # | Stage | Responsibility |
|---|-------|---------------|
| 1 | `observe_stage` | Perceive environment; collect available context |
| 2 | `understand_stage` | Parse and classify incoming input |
| 3 | `read_stage` | Retrieve relevant context from memory or blackboard |
| 4 | `code_intel_stage` | Analyze code artifacts if present in context |
| 5 | `locate_stage` | Identify tools and capabilities relevant to the task |
| 6 | `inject_stage` | Apply prompt injection rules and system constraints |
| 7 | `reason_stage` | Execute LLM inference to produce a response or action plan |
| 8 | `transform_stage` | Post-process and format LLM output |
| 9 | `validate_stage` | Verify output correctness; re-enter loop or emit result |
| — | `respond_stage` | Emit final output through the event bus |

#### 3.1.3 Action Types

The engine dispatches 13+ built-in action types during reasoning:

| Action | Description |
|--------|-------------|
| `bash_action` | Execute shell commands in a sandboxed subprocess |
| `read_action` | Read file contents or symbol definitions |
| `write_action` | Write or create files |
| `edit_action` | Apply targeted edits to existing code or text |
| `glob_action` | Find files matching a pattern |
| `grep_action` | Search file contents via regex |
| `web_fetch_action` | Make HTTP requests to external URLs |
| `web_search_action` | Perform internet searches |
| `mcp_tool_action` | Delegate to a connected MCP tool server |
| `messaging_actions` | Send directed messages or broadcasts |
| `blackboard_actions` | Read/write shared state |
| `memory_actions` | Access or append conversation history |
| `task_action` | Spawn or delegate to a sub-task |
| `todo_write_action` | Persist task items across turns |

#### 3.1.4 LLM Provider Support

The engine's `llm_factory` creates provider-specific clients from a uniform config object:

```json
{
  "provider": "<name>",
  "model": "<model-id>",
  "api_key": "<key>",
  "base_url": "<optional-override>"
}
```

Supported providers:

| Provider | Type |
|----------|------|
| `anthropic` | Cloud |
| `openai` | Cloud |
| `google` | Cloud |
| `groq` | Cloud |
| `mistral` | Cloud |
| `deepseek` | Cloud |
| `xai` | Cloud |
| `openrouter` | Cloud aggregator |
| `together` | Cloud aggregator |
| `ollama` | Local |
| `llamacpp` | Local |
| `lmstudio` | Local |
| `vllm` | Local |
| `llama` | Local |
| `custom` | User-defined base URL |
| `mock` | Testing/offline |

#### 3.1.5 Event Bus

The engine emits typed events on callbacks registered via `am_subscribe_events`. Events fire on engine-internal threads; callers must marshal to their own thread/event loop before updating UI.

| Event | Payload |
|-------|---------|
| `agent_started` | `{agent_id, timestamp}` |
| `work_item_started` | `{agent_id, work_item_id, prompt}` |
| `agent_finished` | `{agent_id, result, duration_ms}` |
| `agent_failed` | `{agent_id, error, code}` |
| `agent_cancelled` | `{agent_id}` |
| `blackboard_updated` | `{key, value}` |
| `mcp_connected` | `{server_name, tool_count}` |
| `mcp_disconnected` | `{server_name}` |
| `quota_exceeded` | `{agent_id, provider, limit_type}` |

---

### 3.2 Flutter GUI — Agent Studio

**Location**: `agent_studio/`  
**Language**: Dart  
**Framework**: Flutter  
**Target platforms**: Desktop (Linux, macOS, Windows), Web  
**State management**: `provider` package (ChangeNotifier)  

#### 3.2.1 Application Entry Point

`lib/main.dart` bootstraps three providers and routes to `LoginScreen`. After successful auth, the app routes to `AdminShell` (full permissions) or `UserShell` (read/run only) based on the authenticated role.

```
main()
  └── MultiProvider
        ├── AuthProvider
        ├── AgentProvider
        └── CloudProvider
              └── MaterialApp (dark theme)
                    └── Auth gate → LoginScreen | AdminShell | UserShell
```

#### 3.2.2 Screens

**DashboardScreen** (`screens/dashboard_screen.dart`)  
Root screen presented after login. Contains six tab panels:

| Tab | Content |
|-----|---------|
| Agents | List of all agents with status; create/edit/delete actions |
| Groups | Collaboration groups; create/edit; member assignment |
| Hierarchy | Parent-child agent relationships; drag-and-drop canvas |
| Benchmark | Leaderboard of benchmark runs; per-run metric breakdown |
| Tasks | Task history with prompt, target, status, duration |
| Logs | Live event stream from the engine for debugging |

**AgentBuilderDialog** (`screens/agent_builder_dialog.dart`)  
A 4-step wizard for creating or editing an agent:

1. **Identity** — Name, role selection (orchestrator / worker / specialist / reviewer / planner)
2. **Instructions** — System prompt text editor
3. **Model** — Provider and model selection dropdowns; API key input
4. **Review** — Summary before saving

**GroupBuilderDialog** (`screens/group_builder_dialog.dart`)  
Dialog for creating or editing an agent group:

- Name input
- Collaboration mode selection (5 modes, see §3.2.6)
- Member agent multi-select
- Edge mapping for sequential and pipeline modes

**BenchmarkScreen** (`screens/benchmark_screen.dart`)  
Displays benchmark results in a leaderboard table. Columns: Agent, Model, Latency (ms), TTFT (ms), Tokens/s, Total Tokens, Cost (USD), Quality Score, Success Rate. Supports filtering by agent and sorting by any metric.

**SettingsPanel** (`screens/settings_panel.dart`)  
Controls:

- Connection mode toggle: FFI (desktop) / HTTP (web) / Mock
- MCP server URL list (add/remove)
- Default LLM provider and model
- Per-provider API key storage

**LoginScreen** (`screens/login_screen.dart`)  
Handles email/password and Google OAuth 2.1 sign-in flows. Integrates with the C++ Auth Server.

#### 3.2.3 Providers (State Management)

**AgentProvider** (`providers/agent_provider.dart`)  
Central state manager. Owns:

- `agents: Map<String, AgentModel>` — all registered agents
- `groups: Map<String, AgentGroup>` — all groups
- `tasks: List<TaskModel>` — task history
- `benchmarks: List<BenchmarkResult>` — benchmark results
- `activeConvId` — ID of the agent/group currently displayed in ChatPanel
- `activeIsGroup` — whether the active conversation target is a group

Operations exposed to the UI:

| Method | Description |
|--------|-------------|
| `createAgent(config)` | Spawn a new agent via the backend |
| `updateAgent(id, config)` | Update agent configuration |
| `deleteAgent(id)` | Destroy an agent |
| `runAgent(id, prompt)` | Execute an agent and stream events |
| `cancelAgent(id)` | Cancel a running agent |
| `createGroup(config)` | Create a new agent group |
| `updateGroup(id, config)` | Update group configuration |
| `deleteGroup(id)` | Delete a group |
| `runGroup(id, prompt)` | Execute a group collaboration |
| `pipeAgents(fromId, toId)` | Connect agent output to another agent's input |
| `unpipeAgents(fromId, toId)` | Disconnect a pipe |
| `runBenchmark(agentIds, prompt)` | Run the same prompt across multiple agents and collect metrics |
| `sendMessage(fromId, toId, msg)` | Direct agent-to-agent message |
| `broadcast(fromId, msg)` | Broadcast to all agents |

**AuthProvider** (`providers/auth_provider.dart`)  
Manages authentication state:

- Login / logout via OAuth 2.1 + PKCE
- Google Sign-In integration
- Role: `admin` or `user`
- Stores and refreshes access tokens

**CloudProvider** (`providers/cloud_provider.dart`)  
Manages cloud-specific settings and deployed-agent registry.

#### 3.2.4 Backend Abstraction

`AgentApiService` wraps an `AgentBackend` interface. The concrete implementation is selected at startup based on platform and connection mode.

| Backend | When Used | Transport |
|---------|-----------|-----------|
| `FfiBackend` | Desktop, FFI mode | Direct C FFI calls to `libagent_engine.so` |
| `HttpBackend` | Web, or desktop HTTP mode | REST + WebSocket to C++ server |
| `MockBackend` | Testing, offline demo | In-process simulated responses |

`FfiBackend` calls the C API functions via Dart FFI bindings defined in `lib/ffi/agent_engine_bindings.dart`. `HttpBackend` maps each operation to the corresponding REST endpoint and subscribes to the `/events` WebSocket for live events.

#### 3.2.5 FFI Bindings

`lib/ffi/agent_engine_bindings.dart` declares Dart native function types that map to every C API function. The library is loaded dynamically at startup:

```
DynamicLibrary.open('libagent_engine.so')  // Linux
DynamicLibrary.open('libagent_engine.dylib')  // macOS
```

All string arguments are converted to null-terminated UTF-8 via `toNativeUtf8()`. All result strings from the engine are freed via the corresponding `am_free_string()` call.

#### 3.2.6 Agent Group Collaboration Modes

| Mode | Behaviour |
|------|-----------|
| `parallel` | All agents receive the same prompt simultaneously; results are collected independently |
| `sequential` | Agents execute one after another; each receives the previous agent's output |
| `broadcast` | A lead agent broadcasts a message; all others respond independently |
| `consensus` | All agents produce a response; a judge agent picks or merges the best answer |
| `pipeline` | Agents are wired in a directed graph via `am_pipe()`; data flows along defined edges |

#### 3.2.7 Widgets

| Widget | Description |
|--------|-------------|
| `agent_card` | Compact tile showing name, role, model, and status with action buttons |
| `chat_panel` | Full-height conversation panel with markdown rendering and input box |
| `hierarchy_tree` | Visual tree of parent/child agent relationships |
| `status_badge` | Animated badge with five states: idle, running, waiting, done, error |
| `benchmark_chart` | Radar or bar chart of benchmark metrics for a single run |
| `group_card` | Compact tile for groups with mode label and member count |
| `task_tile` | List tile for a completed or running task |
| `log_entry` | Formatted event log line with timestamp, event type, and payload |
| `model_selector` | Dropdown combining provider and model into a searchable list |
| `provider_key_field` | Secure text field for API key input |
| `connection_mode_toggle` | Segmented button: FFI / HTTP / Mock |

---

### 3.3 C++ Agent Server

**Location**: `agent_server_cpp/` (to be created in Phase 1)  
**Language**: C++17  
**Library**: cpp-httplib, nlohmann/json  
**Purpose**: Thin HTTP/WebSocket wrapper around the embedded C++ Agent Engine. Replaces the Dart `agent_server`.

The server links against `libagent_engine` and calls its C API directly. It is the only backend for the `HttpBackend` in the Flutter app.

See §7 for the full API specification.

---

### 3.4 C++ MCP Tool Server

**Location**: `mcp-server/`  
**Language**: C++17 (HTTP layer) + Python 3 (tool scripts)  
**Port**: 8081  
**Protocol**: JSON-RPC 2.0 over HTTP  

The MCP Tool Server exposes an extensible catalogue of external tools to the Agent Engine via the Model Context Protocol. The C++ layer handles HTTP, authentication, and JSON-RPC dispatch. Each tool is implemented as a standalone Python script that communicates with the dispatcher over stdin/stdout.

#### 3.4.1 Architecture

```
Agent Engine
    │ am_connect_mcp("http://localhost:8081", token)
    ▼
C++ HTTP Server (cpp-httplib)
    │ Bearer token validation → auth-server introspection
    ▼
ToolRegistry
    │ lookup tool from tools_manifest.json
    ▼
ToolRunner
    │ spawn Python subprocess
    │ write JSON input to stdin
    │ read JSON result from stdout
    ▼
Response marshalled back to engine
```

#### 3.4.2 Tool Catalogue

The server registers 50+ tools across categories. Categories include:

| Category | Example Tools |
|----------|--------------|
| Search | web_search, news_search, academic_search |
| Finance | stock_price, crypto_price, exchange_rate |
| Weather | current_weather, forecast, historical_weather |
| Code | code_execute, code_lint, code_format |
| Data | json_parse, csv_parse, xml_parse |
| Communication | email_send, slack_message, sms_send |
| Calendar | calendar_list, calendar_create, calendar_delete |
| Files | file_read, file_write, file_list |
| Utilities | url_fetch, qr_generate, hash_compute |
| AI | image_describe, text_translate, text_summarize |

All tools are declared in `mcp-server/tools/tools_manifest.json` with name, description, category, and JSON Schema for input parameters.

#### 3.4.3 Authentication

Requests to the MCP server must include a Bearer token. The server validates the token by calling the Auth Server's `/introspect` endpoint. Only active tokens with the correct audience are accepted.

#### 3.4.4 Adding New Tools

1. Add a Python script to `mcp-server/tools/<category>/<tool_name>.py`
2. Register the tool in `tools_manifest.json`
3. Inject any required API credentials as environment variables

---

### 3.5 C++ Auth Server

**Location**: `auth-server/`  
**Language**: C++17  
**Port**: 8080  
**Protocol**: OAuth 2.1 with PKCE; RS256 JWT  

Provides identity and access management for the Agent Studio platform. Supports dynamic client registration, PKCE-based authorization code flow, and JWT access token issuance.

#### 3.5.1 Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | `/register` | Dynamic client registration |
| GET | `/authorize` | Authorization endpoint (PKCE challenge) |
| POST | `/token` | Token exchange (code → access + refresh token) |
| POST | `/introspect` | Token introspection for resource servers |
| GET | `/.well-known/oauth-authorization-server` | Server metadata |
| GET | `/.well-known/jwks.json` | Public key set for RS256 verification |

#### 3.5.2 Token Properties

- **Algorithm**: RS256
- **Access token TTL**: configurable (default 1 hour)
- **Refresh token TTL**: configurable (default 30 days)
- **Claims**: `sub`, `aud`, `iat`, `exp`, `role`, `scope`

---

### 3.6 C++ Data Server

**Location**: `data server/`  
**Language**: C++  
**Protocol**: Custom  
**Status**: Active, out of scope for current migration  

Graph-based persistence backend. Stores agent relationship graphs and structured data. Its internal API is not exposed to the Flutter GUI at this time.

---

### 3.7 Dart Agent Server (Decommission Target)

**Location**: `agent_server/`  
**Language**: Dart  
**Framework**: Shelf + shelf_router  
**Port**: 3001  
**Status**: To be deleted in Phase 3 of the migration  

This server was built to provide a REST API for the Flutter web app before the C++ engine had an HTTP interface. It re-implements agent execution and LLM routing independently of the engine, creating dangerous duplication. It will be replaced in full by the C++ Agent Server (§3.3).

**Do not extend this server.** Bug fixes only until decommission.

---

## 4. Functional Requirements

### 4.1 Agent Management (FR-AGT)

| ID | Requirement |
|----|-------------|
| FR-AGT-01 | Users shall be able to create a new agent by specifying: name, role, system prompt, LLM provider, and model. |
| FR-AGT-02 | Users shall be able to view a list of all agents with their current status (idle, running, waiting, done, error). |
| FR-AGT-03 | Users shall be able to edit the configuration of any existing agent. |
| FR-AGT-04 | Users shall be able to delete an agent. Running agents shall be cancelled before deletion. |
| FR-AGT-05 | Users shall be able to run an agent by providing a prompt; results shall stream in real time. |
| FR-AGT-06 | Users shall be able to cancel a running agent. |
| FR-AGT-07 | Users shall be able to view the full conversation history for each agent. |
| FR-AGT-08 | The system shall enforce agent roles: orchestrator, worker, specialist, reviewer, planner. |
| FR-AGT-09 | An orchestrator agent shall be able to delegate tasks to worker or specialist agents. |

### 4.2 Agent Groups (FR-GRP)

| ID | Requirement |
|----|-------------|
| FR-GRP-01 | Users shall be able to create a group from any subset of existing agents. |
| FR-GRP-02 | Users shall be able to select one of five collaboration modes per group (parallel, sequential, broadcast, consensus, pipeline). |
| FR-GRP-03 | In pipeline mode, users shall be able to define directed edges between agents within the group. |
| FR-GRP-04 | Users shall be able to run a group with a prompt; the system shall coordinate agents according to the selected mode. |
| FR-GRP-05 | Group execution results shall be streamed to the GUI in real time. |
| FR-GRP-06 | Users shall be able to edit or delete groups without affecting the member agents. |

### 4.3 Agent Wiring (FR-WIRE)

| ID | Requirement |
|----|-------------|
| FR-WIRE-01 | Users shall be able to connect any two agents via a directed pipe (output of A → input of B). |
| FR-WIRE-02 | Users shall be able to disconnect an existing pipe. |
| FR-WIRE-03 | The Hierarchy tab shall visualize all current agent relationships as a directed graph. |
| FR-WIRE-04 | The system shall prevent cyclic pipes that would cause infinite loops. |

### 4.4 Benchmarking (FR-BENCH)

| ID | Requirement |
|----|-------------|
| FR-BENCH-01 | Users shall be able to submit a single prompt to multiple agents simultaneously for comparative benchmarking. |
| FR-BENCH-02 | The system shall record and display four metric categories per run: speed, cost, quality, reliability. |
| FR-BENCH-03 | Speed metrics shall include: latency (end-to-end ms), TTFT (time-to-first-token ms), throughput (tokens/second). |
| FR-BENCH-04 | Cost metrics shall include: prompt tokens, completion tokens, total tokens, estimated cost (USD). |
| FR-BENCH-05 | Quality metrics shall include a numeric quality score (0–10). Default scoring method is configurable (LLM-as-judge or expected-answer comparison). |
| FR-BENCH-06 | Reliability metrics shall include: success flag, retry count, error type. |
| FR-BENCH-07 | Benchmark results shall be displayed in a sortable leaderboard table. |
| FR-BENCH-08 | Benchmark history shall be persisted for the duration of the session. |

### 4.5 Real-Time Events (FR-EVT)

| ID | Requirement |
|----|-------------|
| FR-EVT-01 | The GUI shall receive engine events in real time with sub-200 ms latency. |
| FR-EVT-02 | Agent status badges shall update immediately upon receiving start/finish/error events. |
| FR-EVT-03 | The Logs tab shall display all engine events in chronological order. |
| FR-EVT-04 | The chat panel shall stream agent output tokens as they are generated. |
| FR-EVT-05 | WebSocket shall be the transport for HTTP-mode clients; FFI callbacks for desktop clients. |

### 4.6 MCP Tool Integration (FR-MCP)

| ID | Requirement |
|----|-------------|
| FR-MCP-01 | The engine shall support connecting one or more MCP tool servers at runtime. |
| FR-MCP-02 | Connected MCP tools shall be automatically available to all agents. |
| FR-MCP-03 | Users shall be able to add or remove MCP server URLs from the Settings panel. |
| FR-MCP-04 | The system shall display the list of connected MCP servers and their tool counts. |
| FR-MCP-05 | MCP tool calls shall be authenticated using Bearer tokens issued by the Auth Server. |

### 4.7 Authentication & Authorisation (FR-AUTH)

| ID | Requirement |
|----|-------------|
| FR-AUTH-01 | Users shall authenticate via OAuth 2.1 with PKCE before accessing any agent features. |
| FR-AUTH-02 | The system shall support Google Sign-In as an OAuth identity provider. |
| FR-AUTH-03 | Admin users shall have full CRUD access to all agents, groups, and settings. |
| FR-AUTH-04 | Standard users shall only be able to run agents and view results; they shall not create or delete agents. |
| FR-AUTH-05 | Access tokens shall expire and be refreshed automatically without re-prompting the user. |

### 4.8 LLM Configuration (FR-LLM)

| ID | Requirement |
|----|-------------|
| FR-LLM-01 | The engine shall be the sole component that calls LLM provider APIs. |
| FR-LLM-02 | Users shall be able to configure the default LLM provider and model from the Settings panel. |
| FR-LLM-03 | Individual agents shall be able to override the default provider and model. |
| FR-LLM-04 | API keys for all providers shall be stored securely and never logged. |
| FR-LLM-05 | The system shall support at least 15 LLM providers (see §3.1.4). |

### 4.9 Shared State (FR-BB)

| ID | Requirement |
|----|-------------|
| FR-BB-01 | Agents shall be able to read and write key-value pairs to a shared blackboard. |
| FR-BB-02 | Blackboard updates shall trigger a `blackboard_updated` event visible in the Logs tab. |
| FR-BB-03 | The blackboard state shall be readable from the GUI for debugging. |

---

## 5. Non-Functional Requirements

### 5.1 Performance (NFR-PERF)

| ID | Requirement |
|----|-------------|
| NFR-PERF-01 | Agent creation (spawn) shall complete in under 100 ms. |
| NFR-PERF-02 | First token from the LLM shall be forwarded to the GUI within 500 ms of the agent receiving its prompt, excluding network latency to the LLM provider. |
| NFR-PERF-03 | The system shall support at least 20 concurrently running agents without degradation. |
| NFR-PERF-04 | The WebSocket event channel shall have a message delivery latency of under 200 ms under normal load. |

### 5.2 Reliability (NFR-REL)

| ID | Requirement |
|----|-------------|
| NFR-REL-01 | The C++ Agent Server shall return HTTP 503 with a descriptive error if the engine is unavailable. |
| NFR-REL-02 | LLM call failures shall be retried up to 3 times with exponential backoff before marking the agent as failed. |
| NFR-REL-03 | The GUI shall reconnect the WebSocket automatically after a disconnect, with no user action required. |
| NFR-REL-04 | Cancelling an agent shall not crash or corrupt the engine state. |

### 5.3 Security (NFR-SEC)

| ID | Requirement |
|----|-------------|
| NFR-SEC-01 | All HTTP endpoints (agent server, MCP server, auth server) shall require a valid Bearer token. |
| NFR-SEC-02 | API keys for LLM providers shall never be transmitted to the GUI; they shall be stored server-side only. |
| NFR-SEC-03 | Bash actions executed by the engine shall run in a restricted environment (no network, limited filesystem). |
| NFR-SEC-04 | JWT tokens shall be signed with RS256; private keys shall not leave the Auth Server process. |
| NFR-SEC-05 | TLS shall be enforced on all inter-service HTTP connections in production. |

### 5.4 Maintainability (NFR-MAINT)

| ID | Requirement |
|----|-------------|
| NFR-MAINT-01 | The C API (`c_api.h`) shall maintain ABI stability; breaking changes require a major version bump. |
| NFR-MAINT-02 | Adding a new LLM provider shall require changes only in `llm_factory.cpp`. |
| NFR-MAINT-03 | Adding a new MCP tool shall require only: one Python script and one entry in `tools_manifest.json`. |
| NFR-MAINT-04 | The `AgentBackend` interface shall be the only change point when switching between FFI, HTTP, and Mock backends. |

### 5.5 Portability (NFR-PORT)

| ID | Requirement |
|----|-------------|
| NFR-PORT-01 | The Flutter GUI shall run without modification on Linux, macOS, Windows, and web browsers. |
| NFR-PORT-02 | On web, the app shall use `HttpBackend` automatically (FFI is not available in the browser). |
| NFR-PORT-03 | The C++ engine and servers shall compile on Linux and macOS without platform-specific code in the application layer. |

---

## 6. Data Models

### 6.1 AgentModel

```json
{
  "id": "uuid-v4",
  "name": "research-specialist",
  "user_id": "user@example.com",
  "role": "specialist",
  "system_prompt": "You are a research specialist...",
  "model": "claude-3-5-sonnet-20241022",
  "provider": "anthropic",
  "status": "idle",
  "parent_id": null,
  "metadata": {
    "pos": [100, 200]
  }
}
```

**Status values**: `idle` | `running` | `waiting` | `done` | `error` | `cancelled`  
**Role values**: `orchestrator` | `worker` | `specialist` | `reviewer` | `planner`

### 6.2 AgentGroup

```json
{
  "id": "uuid-v4",
  "name": "research-team",
  "mode": "sequential",
  "agents": ["agent-id-1", "agent-id-2", "agent-id-3"],
  "edges": {
    "agent-id-1": "agent-id-2",
    "agent-id-2": "agent-id-3"
  }
}
```

**Mode values**: `parallel` | `sequential` | `broadcast` | `consensus` | `pipeline`

### 6.3 TaskModel

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

### 6.4 BenchmarkResult

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

### 6.5 ModelProvider

```json
{
  "id": "anthropic",
  "display_name": "Anthropic",
  "api_key": "**redacted**",
  "base_url": null,
  "models": ["claude-opus-4-8", "claude-sonnet-4-6", "claude-haiku-4-5-20251001"]
}
```

---

## 7. API Specification

### 7.1 C++ Agent Server — REST Endpoints

**Base URL**: `http://<host>:<port>`  
**Auth**: Bearer token in `Authorization` header (validated against auth-server)  
**Content-Type**: `application/json`  

#### Health

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Returns server and engine status |

**Response 200**:
```json
{
  "status": "ok",
  "version": "1.0.0",
  "api_version": "v1",
  "engine_agents": 3
}
```

#### LLM Configuration

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/llm/configure` | Set the default LLM provider and model |

**Request body**:
```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-6",
  "api_key": "<key>"
}
```

#### Agents

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/agents` | List all agents |
| POST | `/api/agents` | Spawn a new agent |
| GET | `/api/agents/{id}` | Get agent status |
| DELETE | `/api/agents/{id}` | Destroy an agent |
| POST | `/api/agents/{id}/run` | Execute an agent with a prompt |
| POST | `/api/agents/{id}/inject` | Inject or override a work item mid-run |
| POST | `/api/agents/{id}/cancel` | Cancel a running agent |

**POST /api/agents** request:
```json
{
  "name": "research-agent",
  "role": "specialist",
  "system_prompt": "You are a research specialist...",
  "provider": "anthropic",
  "model": "claude-sonnet-4-6",
  "api_key": "<key>"
}
```

**POST /api/agents/{id}/run** request:
```json
{
  "prompt": "Research the latest advances in fusion energy."
}
```

**Response** for `/run`: HTTP 200 with result JSON after completion, or WebSocket stream for async.

#### Agent Wiring

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/pipe` | Connect output of one agent to input of another |
| DELETE | `/api/pipe` | Disconnect a pipe |

**POST /api/pipe** request:
```json
{
  "from_id": "agent-id-1",
  "to_id": "agent-id-2"
}
```

#### Messaging

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/agents/{from_id}/send` | Send a directed message to another agent |
| POST | `/api/broadcast` | Broadcast a message to all agents |

#### Blackboard

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/blackboard` | List all blackboard keys |
| GET | `/api/blackboard/{key}` | Read a value |
| POST | `/api/blackboard/{key}` | Write a value |

#### MCP Servers

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/mcp` | List connected MCP servers |
| POST | `/api/mcp` | Connect a new MCP server |
| DELETE | `/api/mcp/{name}` | Disconnect a named MCP server |

**POST /api/mcp** request:
```json
{
  "url": "http://localhost:8081",
  "auth_token": "<bearer-token>"
}
```

#### Benchmarks

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/benchmark` | Run a benchmark across a list of agents |

**POST /api/benchmark** request:
```json
{
  "agent_ids": ["agent-id-1", "agent-id-2"],
  "prompt": "Explain quantum entanglement in simple terms.",
  "quality_method": "llm_judge"
}
```

#### Real-Time Events

| Protocol | Path | Description |
|----------|------|-------------|
| WebSocket | `/events` | Stream of all engine events as JSON messages |

**Event message format**:
```json
{
  "event": "agent_finished",
  "payload": {
    "agent_id": "agent-id-1",
    "result": "...",
    "duration_ms": 4200
  },
  "timestamp": "2026-06-25T10:00:45.123Z"
}
```

### 7.2 MCP Tool Server — JSON-RPC 2.0

| Method | Path | Description |
|--------|------|-------------|
| GET | `/tools` | List all registered tools (manifest) |
| POST | `/rpc` | Execute a tool via JSON-RPC 2.0 |

**POST /rpc** request:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tool/call",
  "params": {
    "name": "web_search",
    "arguments": {
      "query": "latest fusion energy research 2026"
    }
  }
}
```

---

## 8. Event System

### 8.1 Engine Events (via FFI callback or WebSocket)

All events share the envelope:
```json
{
  "event": "<event_type>",
  "payload": { ... },
  "timestamp": "ISO-8601"
}
```

| Event Type | When Fired | Key Payload Fields |
|------------|-----------|-------------------|
| `agent_started` | Agent begins execution | `agent_id` |
| `work_item_started` | Agent begins processing a specific work item | `agent_id`, `work_item_id`, `prompt` |
| `agent_finished` | Agent completes successfully | `agent_id`, `result`, `duration_ms` |
| `agent_failed` | Agent encounters an unrecoverable error | `agent_id`, `error`, `code` |
| `agent_cancelled` | Agent is cancelled by user | `agent_id` |
| `blackboard_updated` | Any agent writes to the blackboard | `key`, `value` |
| `mcp_connected` | An MCP server is successfully attached | `server_name`, `tool_count` |
| `mcp_disconnected` | An MCP server is removed | `server_name` |
| `quota_exceeded` | An agent hits a provider rate limit | `agent_id`, `provider`, `limit_type` |

### 8.2 GUI Event Handling

In `FfiBackend`, the C callback fires on an engine thread. The callback posts the event JSON to a Dart `ReceivePort` via `NativePort` so that it is processed on the Dart isolate's event loop before `notifyListeners()` is called.

In `HttpBackend`, the WebSocket message arrives on the Dart isolate directly and is decoded and dispatched to `AgentProvider`.

---

## 9. Integration Requirements

### 9.1 Engine ↔ MCP Server

- The engine calls `am_connect_mcp(url, token)` once per MCP server at startup or when the user adds one in Settings.
- The engine resolves MCP tool calls during `mcp_tool_action` stage execution.
- The engine passes API credentials for each tool as environment variables when spawning tool subprocesses; credentials are never embedded in JSON-RPC payloads.

### 9.2 MCP Server ↔ Auth Server

- Every inbound request to the MCP server is validated via a `POST /introspect` call to the Auth Server.
- The MCP server caches introspection results for 60 seconds to reduce latency.

### 9.3 Flutter ↔ Engine (FFI)

- The Dart isolate that owns the FFI backend shall not block waiting on engine calls; all blocking calls (`am_future_wait`) shall run in a `compute` isolate or separate thread.
- Events from the engine shall be marshalled to the UI isolate via `ReceivePort`.

### 9.4 Flutter ↔ C++ Server (HTTP)

- The `HttpBackend` shall maintain a persistent WebSocket connection to `/events` for the lifetime of the session.
- REST calls shall include the Bearer token in `Authorization: Bearer <token>`.
- Connection failures shall trigger automatic reconnect with exponential backoff (2 s, 4 s, 8 s, 16 s, up to 4 retries).

---

## 10. Deployment Requirements

### 10.1 Development Stack

```bash
./start.sh              # Build all C++ components + launch all services + Cloudflare tunnel
./start.sh --no-build   # Skip compilation; start services only
./start.sh --no-tunnel  # No public tunnel (LAN only)
./start.sh --stop       # Stop all services
```

Services launched:

| Service | URL |
|---------|-----|
| Flutter Web GUI | http://0.0.0.0:8080 |
| C++ Agent Server | http://0.0.0.0:<TBD> |
| C++ MCP Tool Server | http://0.0.0.0:8081 |
| C++ Auth Server | http://0.0.0.0:8080 |
| Public tunnel | Cloudflare-assigned URL |

### 10.2 Docker Compose

`docker-compose.yml` builds and runs:
- Flutter web GUI (via `Dockerfile.web`)
- C++ Agent Server (once implemented)
- nginx reverse proxy

```bash
docker compose up --build
```

### 10.3 Environment Variables

| Variable | Component | Purpose |
|----------|-----------|---------|
| `MCP_AUTH_TOKEN` | MCP server | Bearer token for inbound requests |
| `AUTH_SERVER_URL` | MCP server | URL for token introspection |
| `OPENAI_API_KEY` | MCP server | OpenAI tools |
| `ANTHROPIC_API_KEY` | MCP server | Anthropic tools |
| `GOOGLE_API_KEY` | MCP server | Google tools |
| `JWT_PRIVATE_KEY_PATH` | Auth server | RS256 signing key |
| `JWT_PUBLIC_KEY_PATH` | Auth server | RS256 verification key |
| `GOOGLE_CLIENT_ID` | Auth server / Flutter | Google OAuth |

---

## 11. Migration Phases

The platform is undergoing a consolidation to eliminate the duplicate Dart server and direct LLM calls in the GUI. The migration is defined in `ARCHITECTURE_PLAN.md` and tracked in `docs/requirements/`.

| Phase | Goal | Status |
|-------|------|--------|
| 1 | Build C++ Agent Server (`agent_server_cpp/`) wrapping the engine C API; WebSocket `/events`; `GET /api/llm/models`; centralized auth introspection | Planned |
| 2 | Delete Dart `agent_server/` and `mcp_server/` directories | Planned |
| 3 | Clean `start.sh` and `docker-compose.yml` of all Dart build and run steps | Planned |
| 4 | Add LLM-as-judge quality scorer to benchmark endpoint; configurable via `JUDGE_MODEL` / `JUDGE_PROVIDER` env vars | Planned |
| 5 | GUI (deferred — to be scoped separately when resumed) | Deferred |

---

## 12. Architectural Decisions (Resolved)

All four decisions are locked. No further discussion needed — implement exactly as specified below.

| # | Decision | Resolution |
|---|----------|------------|
| 1 | **Event transport** | **WebSocket** — C++ Agent Server opens a persistent two-way WebSocket connection at `WS /events`. Engine event callbacks are marshalled from engine threads into a thread-safe queue; a dedicated sender thread drains the queue and writes to all connected clients. |
| 2 | **Model list** | **Dynamic endpoint** — Agent Server exposes `GET /api/llm/models`. For each configured provider the server queries the provider's model-list API and returns the live result. Response schema: `{"provider": "<name>", "models": ["model-id", ...]}[]`. |
| 3 | **Benchmark quality scoring** | **LLM-as-judge** — After each agent completes a benchmark run, the Agent Server sends the original prompt and the agent's response to a designated judge model (configurable via `JUDGE_MODEL` and `JUDGE_PROVIDER` env vars). The judge is prompted to return a numeric score 0–10 as JSON. That score is stored in `BenchmarkResult.quality.score` with `"method": "llm_judge"`. |
| 4 | **Auth enforcement on Agent Server** | **Centralized introspection** — The Agent Server calls `POST <AUTH_SERVER_URL>/introspect` on every inbound request, identical to how the MCP Tool Server does it. Valid token responses are cached in-process for 60 seconds. Returns HTTP 401 on failure. |

---

*End of Requirements Document*
