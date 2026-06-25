# Agent Studio — Software Requirements Document

**Version**: 2.0
**Date**: 2026-06-25
**Status**: Active

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [System Architecture](#2-system-architecture)
3. [Component Descriptions](#3-component-descriptions)
   - 3.1 [C++ Agent Engine](#31-c-agent-engine)
   - 3.2 [React Web UI](#32-react-web-ui)
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
12. [Architectural Decisions (Resolved)](#12-architectural-decisions-resolved)

---

## 1. Project Overview

**Agent Studio** is a multi-model AI agent orchestration platform. It provides a web interface for creating, configuring, running, and benchmarking autonomous AI agents. Agents can operate individually, be wired together in directed pipelines, or collaborate in groups using one of five coordination modes.

### Goals

- Provide a single unified platform for managing agents across multiple LLM (Large Language Model) providers.
- Consolidate all LLM inference calls into the C++ agent engine — no direct LLM calls from the UI or intermediate servers.
- Deliver the UI as a browser-based React application that communicates with the C++ backend over REST and WebSocket.
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
  Browser (React Web UI)
       │
       ├── REST calls (TanStack Query)  →  C++ Agent Server
       └── WebSocket /events            →  C++ Agent Server
                                                  │
                                           embeds via C API
                                                  │
                                        C++ Agent Engine (libagent_engine)
                                                  │
                                          am_connect_mcp
                                                  │
                                        C++ MCP Tool Server (port 8081)
                                                  │
                                        token introspection
                                                  │
                                        C++ Auth Server (port 8080)
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
| React Web UI | TypeScript/React | 5173 dev / 80 prod | Planned (Phase 1) |
| Dart Agent Server | Dart | 3001 | Decommission target |
| Dart MCP Server | Dart | 3000 | Decommission target |

---

## 3. Component Descriptions

### 3.1 C++ Agent Engine

**Location**: `agent/`
**Language**: C++17
**Build**: CMake 3.17+, `-DAGENT_ENABLE_API_LLM=ON` required for cloud LLM support

The agent engine is the core runtime of the platform. It is compiled as a shared library (`libagent_engine.so`) and exposed via a stable C ABI (`agent/include/agent_engine/c_api.h`). All LLM inference, agent lifecycle, tool execution, inter-agent communication, and event emission live here.

#### 3.1.1 C Public API

**Lifecycle**

| Function | Description |
|----------|-------------|
| `am_create()` | Instantiate a new AgentManager |
| `am_destroy(handle)` | Shut down and free an AgentManager |
| `am_free_string(str)` | Free a string returned by the engine |

**Agent CRUD**

| Function | Description |
|----------|-------------|
| `am_spawn_agent(handle, config_json)` | Create and register a new agent; returns agent ID |
| `am_destroy_agent(handle, agent_id)` | Stop and unregister an agent |
| `am_list_agents(handle)` | Return JSON array of all agents with status |
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

The engine's `llm_factory` creates provider-specific clients from a uniform config:

```json
{
  "provider": "<name>",
  "model": "<model-id>",
  "api_key": "<key>",
  "base_url": "<optional-override>"
}
```

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

The engine emits typed events on callbacks registered via `am_subscribe_events`. Events fire on engine-internal threads; the C++ Agent Server marshals them to a thread-safe queue before forwarding over WebSocket.

| Event | Key Payload Fields |
|-------|--------------------|
| `agent_started` | `agent_id`, `timestamp` |
| `work_item_started` | `agent_id`, `work_item_id`, `prompt` |
| `agent_finished` | `agent_id`, `result`, `duration_ms` |
| `agent_failed` | `agent_id`, `error`, `code` |
| `agent_cancelled` | `agent_id` |
| `blackboard_updated` | `key`, `value` |
| `mcp_connected` | `server_name`, `tool_count` |
| `mcp_disconnected` | `server_name` |
| `quota_exceeded` | `agent_id`, `provider`, `limit_type` |

---

### 3.2 React Web UI

**Location**: `ui/`
**Language**: TypeScript
**Framework**: React 18
**Build tool**: Vite
**Target**: Modern browsers (Chrome, Firefox, Safari, Edge)

The React UI is the only client of the C++ Agent Server. It communicates exclusively over REST and WebSocket — there is no direct connection to the engine. All state comes from the server; the UI holds no business logic.

#### 3.2.1 Tech Stack

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
| Auth | OAuth 2.1 + PKCE | Token flow against the C++ Auth Server |

#### 3.2.2 Directory Structure

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
│   │   ├── Dashboard.tsx      # Root layout with tab navigation
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
│   │   ├── AuthContext.tsx    # Token storage, refresh, role
│   │   └── LoginPage.tsx      # OAuth 2.1 + PKCE login flow
│   └── main.tsx
├── index.html
├── vite.config.ts
├── tailwind.config.ts
├── tsconfig.json
└── package.json
```

#### 3.2.3 Pages

**Dashboard** — Root layout with six tabs:

| Tab | Content |
|-----|---------|
| Agents | Agent list with status badges; create/edit/delete; run with prompt |
| Groups | Group list; create/edit; run with prompt |
| Hierarchy | Visual directed graph of pipe and parent/child relationships |
| Benchmark | Run same prompt across multiple agents; sortable leaderboard |
| Tasks | Task history: prompt, target, status, duration, result |
| Logs | Live scrolling engine event feed with timestamps |

**Settings** — Controls:
- MCP server URL list (add/remove)
- Default LLM provider and model (populated dynamically from `GET /api/llm/models`)
- Per-provider API key input fields

**Agent create/edit drawer** — 4-step form:
1. Name and role (orchestrator / worker / specialist / reviewer / planner)
2. System prompt text editor
3. Provider and model selection (dynamic list from server)
4. Review and save

**Group create/edit drawer**:
- Name, collaboration mode (5 modes), member agent multi-select, edge mapping

#### 3.2.4 WebSocket Event Handling

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

Events update `eventStore.ts`. TanStack Query invalidates relevant caches on `agent_finished` and `agent_failed` so lists refresh automatically.

#### 3.2.5 Authentication

OAuth 2.1 + PKCE flow against the C++ Auth Server:

1. User clicks login → UI generates code verifier and code challenge
2. Redirect to `GET /authorize` with `code_challenge` and `code_challenge_method=S256`
3. Auth Server returns an authorization code
4. UI exchanges code for access + refresh token via `POST /token`
5. Access token stored in memory only (not localStorage); refresh token in HTTP-only cookie
6. Every API request includes `Authorization: Bearer <token>`
7. On 401 → silently refresh token and retry once

#### 3.2.6 Agent Group Collaboration Modes

| Mode | How it works |
|------|-------------|
| `parallel` | All agents receive the same prompt simultaneously; results collected independently |
| `sequential` | Agents execute one after another; each receives the previous agent's output |
| `broadcast` | Lead agent broadcasts; all others respond independently |
| `consensus` | All agents respond; a reviewer agent selects or merges the best answer |
| `pipeline` | Agents wired via directed edges; output flows along the defined graph |

---

### 3.3 C++ Agent Server

**Location**: `agent_server_cpp/` (to be created in Phase 1)
**Language**: C++17
**Libraries**: cpp-httplib, nlohmann/json
**Purpose**: Thin HTTP and WebSocket wrapper around the embedded C++ Agent Engine. Links directly against `libagent_engine` and is the sole backend the React UI talks to.

See §7 for the full API specification.

---

### 3.4 C++ MCP Tool Server

**Location**: `mcp-server/`
**Language**: C++17 (HTTP/dispatch layer) + Python 3 (tool scripts)
**Port**: 8081
**Protocol**: JSON-RPC 2.0 over HTTP

Exposes 50+ external tools to the Agent Engine. The C++ layer handles HTTP, authentication, and dispatch. Each tool is a standalone Python script communicating over stdin/stdout.

#### 3.4.1 Tool Catalogue

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

#### 3.4.2 Authentication

Every inbound request validated via `POST /introspect` on the Auth Server. Results cached for 60 seconds.

---

### 3.5 C++ Auth Server

**Location**: `auth-server/`
**Language**: C++17
**Port**: 8080
**Protocol**: OAuth 2.1 with PKCE; RS256 JWT

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

| Property | Value |
|----------|-------|
| Algorithm | RS256 |
| Access token TTL | 1 hour (configurable) |
| Refresh token TTL | 30 days (configurable) |
| Claims | `sub`, `aud`, `iat`, `exp`, `role`, `scope`, `jti` |

---

### 3.6 C++ Data Server

**Location**: `data server/`
**Language**: C++
**Status**: Active — out of scope for current work

Graph-based persistence backend. Not directly accessible from the React UI at this time.

---

### 3.7 Dart Agent Server (Decommission Target)

**Location**: `agent_server/`
**Language**: Dart / Shelf
**Port**: 3001
**Status**: Delete in Phase 2

Re-implements agent execution and LLM routing independently of the C++ engine. **Do not extend — bug fixes only.**

---

## 4. Functional Requirements

### 4.1 Agent Management (FR-AGT)

| ID | Requirement |
|----|-------------|
| FR-AGT-01 | Users shall create a new agent specifying: name, role, system prompt, LLM provider, and model. |
| FR-AGT-02 | Users shall view all agents with their current status. |
| FR-AGT-03 | Users shall edit the configuration of any existing agent. |
| FR-AGT-04 | Users shall delete an agent; running agents shall be cancelled first. |
| FR-AGT-05 | Users shall run an agent with a prompt; results stream in real time. |
| FR-AGT-06 | Users shall cancel a running agent. |
| FR-AGT-07 | Users shall view the full conversation history for each agent. |
| FR-AGT-08 | The system shall enforce roles: orchestrator, worker, specialist, reviewer, planner. |
| FR-AGT-09 | An orchestrator agent shall delegate tasks to worker or specialist agents. |

### 4.2 Agent Groups (FR-GRP)

| ID | Requirement |
|----|-------------|
| FR-GRP-01 | Users shall create a group from any subset of existing agents. |
| FR-GRP-02 | Users shall select one of five collaboration modes per group. |
| FR-GRP-03 | In pipeline mode, users shall define directed edges between agents. |
| FR-GRP-04 | Users shall run a group with a prompt; agents coordinate per the selected mode. |
| FR-GRP-05 | Group execution results shall stream to the UI in real time. |
| FR-GRP-06 | Users shall edit or delete groups without affecting member agents. |

### 4.3 Agent Wiring (FR-WIRE)

| ID | Requirement |
|----|-------------|
| FR-WIRE-01 | Users shall connect two agents via a directed pipe (output of A → input of B). |
| FR-WIRE-02 | Users shall disconnect an existing pipe. |
| FR-WIRE-03 | The Hierarchy page shall visualize all agent relationships as a directed graph. |
| FR-WIRE-04 | The system shall prevent cyclic pipes that would cause infinite loops. |

### 4.4 Benchmarking (FR-BENCH)

| ID | Requirement |
|----|-------------|
| FR-BENCH-01 | Users shall submit one prompt to multiple agents simultaneously for comparison. |
| FR-BENCH-02 | The system shall collect four metric categories: speed, cost, quality, reliability. |
| FR-BENCH-03 | Speed: latency (ms), time-to-first-token (ms), throughput (tokens/sec). |
| FR-BENCH-04 | Cost: prompt tokens, completion tokens, total tokens, estimated cost (USD). |
| FR-BENCH-05 | Quality: numeric score 0–10 from an LLM-as-judge model. |
| FR-BENCH-06 | Reliability: success flag, retry count, error type. |
| FR-BENCH-07 | Results shall display in a sortable leaderboard table. |

### 4.5 Real-Time Events (FR-EVT)

| ID | Requirement |
|----|-------------|
| FR-EVT-01 | The UI shall receive engine events in real time with sub-200 ms latency. |
| FR-EVT-02 | Agent status badges shall update immediately on start/finish/error events. |
| FR-EVT-03 | The Logs page shall display all engine events in chronological order. |
| FR-EVT-04 | The chat panel shall stream agent output tokens as they arrive. |
| FR-EVT-05 | WebSocket shall be the sole event transport between UI and Agent Server. |

### 4.6 MCP Tool Integration (FR-MCP)

| ID | Requirement |
|----|-------------|
| FR-MCP-01 | The engine shall support connecting one or more MCP tool servers at runtime. |
| FR-MCP-02 | Connected MCP tools shall be automatically available to all agents. |
| FR-MCP-03 | Users shall add or remove MCP server URLs from the Settings page. |
| FR-MCP-04 | The UI shall display connected MCP servers and their tool counts. |
| FR-MCP-05 | MCP tool calls shall be authenticated using Bearer tokens from the Auth Server. |

### 4.7 Authentication (FR-AUTH)

| ID | Requirement |
|----|-------------|
| FR-AUTH-01 | Users shall authenticate via OAuth 2.1 with PKCE before accessing any feature. |
| FR-AUTH-02 | Admin users shall have full CRUD access to agents, groups, and settings. |
| FR-AUTH-03 | Standard users shall only run agents and view results. |
| FR-AUTH-04 | Access tokens shall refresh silently without re-prompting the user. |

### 4.8 LLM Configuration (FR-LLM)

| ID | Requirement |
|----|-------------|
| FR-LLM-01 | The engine shall be the sole component that calls LLM provider APIs. |
| FR-LLM-02 | Users shall configure the default LLM provider and model from Settings. |
| FR-LLM-03 | The model list in the UI shall be populated dynamically from `GET /api/llm/models`. |
| FR-LLM-04 | Individual agents shall override the default provider and model. |
| FR-LLM-05 | API keys shall never be logged or returned in API responses. |

### 4.9 Shared State / Blackboard (FR-BB)

| ID | Requirement |
|----|-------------|
| FR-BB-01 | Agents shall read and write key-value pairs to a shared blackboard. |
| FR-BB-02 | Blackboard updates shall emit a `blackboard_updated` event to all subscribers. |
| FR-BB-03 | The UI shall display current blackboard state on the Logs page for debugging. |

---

## 5. Non-Functional Requirements

### 5.1 Performance (NFR-PERF)

| ID | Requirement |
|----|-------------|
| NFR-PERF-01 | Agent spawn shall complete in under 100 ms. |
| NFR-PERF-02 | First token forwarded to the UI within 500 ms of the agent receiving its prompt, excluding provider network latency. |
| NFR-PERF-03 | The system shall support at least 20 concurrently running agents without degradation. |
| NFR-PERF-04 | WebSocket event delivery shall have under 200 ms latency under normal load. |
| NFR-PERF-05 | The React UI initial load shall complete in under 3 seconds on a standard broadband connection. |

### 5.2 Reliability (NFR-REL)

| ID | Requirement |
|----|-------------|
| NFR-REL-01 | The Agent Server shall return HTTP 503 with a descriptive error if the engine is unavailable. |
| NFR-REL-02 | LLM call failures shall be retried up to 3 times with exponential backoff (1 s, 2 s, 4 s). |
| NFR-REL-03 | The UI shall reconnect the WebSocket automatically after a disconnect. |
| NFR-REL-04 | Cancelling an agent shall not corrupt engine state or affect other running agents. |

### 5.3 Security (NFR-SEC)

| ID | Requirement |
|----|-------------|
| NFR-SEC-01 | All HTTP endpoints shall require a valid Bearer token. |
| NFR-SEC-02 | LLM API keys shall be stored server-side only; never returned to the UI. |
| NFR-SEC-03 | Bash actions executed by the engine shall run in a restricted environment. |
| NFR-SEC-04 | JWT tokens shall be signed with RS256; private keys shall not leave the Auth Server. |
| NFR-SEC-05 | TLS shall be enforced on all inter-service HTTP connections in production. |
| NFR-SEC-06 | Access tokens shall be stored in memory only in the browser — not in localStorage. |

### 5.4 Maintainability (NFR-MAINT)

| ID | Requirement |
|----|-------------|
| NFR-MAINT-01 | The C API (`c_api.h`) shall maintain ABI stability; breaking changes require a major version bump. |
| NFR-MAINT-02 | Adding a new LLM provider shall require changes only in `llm_factory.cpp`. |
| NFR-MAINT-03 | Adding a new MCP tool shall require only one Python script and one manifest entry. |
| NFR-MAINT-04 | TypeScript types in `ui/src/types/` shall match the JSON schemas in §6 exactly. |

### 5.5 Portability (NFR-PORT)

| ID | Requirement |
|----|-------------|
| NFR-PORT-01 | The React UI shall run in Chrome, Firefox, Safari, and Edge without modification. |
| NFR-PORT-02 | The C++ engine and servers shall compile on Linux and macOS. |
| NFR-PORT-03 | The React UI shall be deployable as a static site behind any web server or CDN. |

---

## 6. Data Models

### 6.1 Agent

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

`status`: `idle` | `running` | `waiting` | `done` | `error` | `cancelled`
`role`: `orchestrator` | `worker` | `specialist` | `reviewer` | `planner`

### 6.2 AgentGroup

```json
{
  "id": "uuid-v4",
  "name": "research-team",
  "mode": "sequential",
  "agents": ["agent-id-1", "agent-id-2"],
  "edges": { "agent-id-1": "agent-id-2" }
}
```

`mode`: `parallel` | `sequential` | `broadcast` | `consensus` | `pipeline`

### 6.3 Task

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
  "agent_name": "Claude Agent",
  "model": "claude-sonnet-4-6",
  "provider": "anthropic",
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

## 7. API Specification

### 7.1 C++ Agent Server — REST Endpoints

**Auth**: `Authorization: Bearer <token>` on every request
**Content-Type**: `application/json`

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Server and engine status |
| POST | `/api/llm/configure` | Set default LLM provider and model |
| GET | `/api/llm/models` | Live model list from each configured provider |
| GET | `/api/agents` | List all agents |
| POST | `/api/agents` | Spawn a new agent |
| GET | `/api/agents/{id}` | Get agent status |
| DELETE | `/api/agents/{id}` | Destroy an agent |
| POST | `/api/agents/{id}/run` | Execute agent with a prompt |
| POST | `/api/agents/{id}/inject` | Inject a work item mid-run |
| POST | `/api/agents/{id}/cancel` | Cancel a running agent |
| POST | `/api/pipe` | Connect output of one agent to input of another |
| DELETE | `/api/pipe` | Disconnect a pipe |
| POST | `/api/agents/{from_id}/send` | Send a directed message |
| POST | `/api/broadcast` | Broadcast a message to all agents |
| GET | `/api/blackboard` | List all blackboard keys |
| GET | `/api/blackboard/{key}` | Read a value |
| POST | `/api/blackboard/{key}` | Write a value |
| GET | `/api/mcp` | List connected MCP servers |
| POST | `/api/mcp` | Connect a new MCP server |
| DELETE | `/api/mcp/{name}` | Disconnect a named MCP server |
| POST | `/api/benchmark` | Run a benchmark across multiple agents |
| WS | `/events` | WebSocket stream of all engine events |

### 7.2 MCP Tool Server — JSON-RPC 2.0

| Method | Path | Description |
|--------|------|-------------|
| GET | `/tools` | List all registered tools |
| POST | `/rpc` | Execute a tool |

---

## 8. Event System

All events use the envelope:
```json
{ "event": "<type>", "payload": { ... }, "timestamp": "ISO-8601" }
```

| Event Type | When Fired | Key Payload Fields |
|------------|-----------|-------------------|
| `agent_started` | Agent begins execution | `agent_id` |
| `work_item_started` | Agent begins a work item | `agent_id`, `work_item_id`, `prompt` |
| `agent_finished` | Agent completes successfully | `agent_id`, `result`, `duration_ms` |
| `agent_failed` | Unrecoverable error | `agent_id`, `error`, `code` |
| `agent_cancelled` | Agent is cancelled | `agent_id` |
| `blackboard_updated` | Any blackboard write | `key`, `value` |
| `mcp_connected` | MCP server attached | `server_name`, `tool_count` |
| `mcp_disconnected` | MCP server removed | `server_name` |
| `quota_exceeded` | Provider rate limit hit | `agent_id`, `provider`, `limit_type` |

Engine event callbacks fire on engine worker threads. The Agent Server places each event into a thread-safe queue; a dedicated sender thread drains the queue and writes to all connected WebSocket clients.

The React UI receives events in `useEvents.ts`, updates `eventStore.ts`, and invalidates relevant TanStack Query caches — causing affected components to re-render automatically.

---

## 9. Integration Requirements

### 9.1 Engine ↔ MCP Server

- The engine calls `am_connect_mcp(url, token)` at startup or on demand.
- MCP tool calls resolved during `mcp_tool_action` stage execution.
- API credentials injected as environment variables into tool subprocesses; never embedded in JSON-RPC payloads.

### 9.2 MCP Server ↔ Auth Server

- Every inbound request validated via `POST /introspect`.
- Results cached for 60 seconds.

### 9.3 Agent Server ↔ Engine

- The Agent Server links directly against `libagent_engine` and calls its C API in-process.
- Blocking calls (`am_future_wait`) dispatched on a thread pool.
- Engine event callbacks marshalled from engine threads to the WebSocket send queue via a thread-safe queue and dedicated sender thread.

### 9.4 React UI ↔ Agent Server

- UI maintains a persistent WebSocket connection to `WS /events` using `reconnecting-websocket`.
- All REST calls include `Authorization: Bearer <token>`.
- On 401 → silently refresh token and retry once.
- WebSocket disconnects trigger auto-reconnect with backoff (2 s, 4 s, 8 s, 16 s).

---

## 10. Deployment Requirements

### 10.1 Development

```bash
# Start all C++ services
./start.sh

# Start the UI dev server (separate terminal)
cd ui && npm install && npm run dev   # http://localhost:5173
```

| Service | URL |
|---------|-----|
| React UI (Vite) | http://localhost:5173 |
| C++ Agent Server | http://localhost:3002 |
| C++ MCP Tool Server | http://localhost:8081 |
| C++ Auth Server | http://localhost:8080 |

### 10.2 Production

```bash
cd ui && npm run build   # Outputs static files to ui/dist/
```

nginx serves `ui/dist/` as static files and reverse-proxies:
- `/api/` and `WS /events` → C++ Agent Server
- `/auth/` → C++ Auth Server
- `/tools/` and `/rpc` → C++ MCP Tool Server

`docker-compose.yml` services: `auth_server`, `mcp_server`, `agent_server`, `nginx`

### 10.3 Environment Variables

**Server-side**

| Variable | Component | Purpose |
|----------|-----------|---------|
| `JWT_PRIVATE_KEY_PATH` | Auth server | RS256 signing key |
| `JWT_PUBLIC_KEY_PATH` | Auth server | RS256 verification key |
| `AUTH_SERVER_URL` | Agent server, MCP server | URL for token introspection |
| `AGENT_SERVER_PORT` | Agent server | Port to listen on |
| `MCP_PORT` | MCP server | Port to listen on |
| `JUDGE_PROVIDER` | Agent server | AI provider for LLM-as-judge scoring |
| `JUDGE_MODEL` | Agent server | Model for LLM-as-judge scoring |
| `JUDGE_API_KEY` | Agent server | API key for the judge model |
| `OPENAI_API_KEY` | MCP server | OpenAI tool credentials |
| `ANTHROPIC_API_KEY` | MCP server | Anthropic tool credentials |
| `GOOGLE_API_KEY` | MCP server | Google tool credentials |

**Client-side (`ui/.env`)**

| Variable | Purpose |
|----------|---------|
| `VITE_API_URL` | Base URL of the C++ Agent Server (e.g. `http://localhost:3002`) |
| `VITE_WS_URL` | WebSocket URL of the C++ Agent Server (e.g. `ws://localhost:3002`) |
| `VITE_AUTH_URL` | Base URL of the C++ Auth Server (e.g. `http://localhost:8080`) |

---

## 11. Migration Phases

| Phase | Goal | Status |
|-------|------|--------|
| 1 | Build C++ Agent Server (`agent_server_cpp/`) and React UI (`ui/`) | Planned |
| 2 | Delete Dart `agent_server/` and `mcp_server/` directories | Planned |
| 3 | Clean `start.sh` and `docker-compose.yml` of all Dart references | Planned |
| 4 | Add LLM-as-judge quality scorer to benchmark endpoint | Planned |
| 5 | Add pipeline canvas (drag-and-drop agent wiring) to React UI | Planned |

---

## 12. Architectural Decisions (Resolved)

| # | Decision | Resolution |
|---|----------|------------|
| 1 | **Event transport** | **WebSocket** — persistent two-way connection at `WS /events`; engine events marshalled through a thread-safe queue to all connected clients |
| 2 | **Model list** | **Dynamic** — `GET /api/llm/models` queries each configured provider live; React UI populates model dropdowns from this endpoint |
| 3 | **Benchmark quality scoring** | **LLM-as-judge** — Agent Server calls a designated judge model after each run; score stored in `BenchmarkResult.quality.score` with `"method": "llm_judge"` |
| 4 | **Auth enforcement** | **Centralized introspection** — Agent Server and MCP Server both call `POST /introspect` on every request; valid responses cached for 60 seconds |
| 5 | **UI technology** | **React + TypeScript** — communicates with the C++ backend exclusively over REST and WebSocket; no Flutter or Dart |

---

*End of Requirements Document*
