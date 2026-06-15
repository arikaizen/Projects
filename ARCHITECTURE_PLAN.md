# Agent Studio — Architecture & Migration Plan

> Status: **Proposed** (for approval). No server/engine code is changed by this
> document. Author target branch: `claude/lucid-heisenberg-dt9vm8` (PR #98).

## 1. Goal

One coherent product:

- **One engine** (the existing C++ `agent/`) runs the agent loop for **all** AI
  models and makes **all** LLM API calls (Claude, GPT, Gemini, Ollama, …)
  through its `AIModel` class via `llm_factory`.
- **One server** (new, C++) embeds the engine and serves the GUI over the
  network for the **web** app.
- **One Flutter app** that is **GUI-only**. It runs the same engine two ways:
  - **Local / desktop:** linked directly via FFI (`libagent_engine`).
  - **Web:** over HTTP + a live event stream to the one C++ server.
- **No LLM calls anywhere in Dart.** The Flutter app never talks to a model
  provider directly.
- The GUI lets you fully control agents, edit their input prompts, **redirect
  one agent's output into another's input by drawing a connector line**, and
  **benchmark agents on every parameter**.

### Design principles
1. **Single source of truth for the loop and LLM = the C++ engine.** Everything
   else is a thin client of the engine's C API.
2. **Delete duplication, don't add abstraction.** Three separate LLM
   implementations exist today; two get deleted.
3. **One transport contract** shared by the FFI backend and the web backend, so
   the Flutter UI code is identical in both modes.

---

## 2. Current state

### Component inventory
| Component | Dir | Lang | Keep? |
|---|---|---|---|
| Agent Engine | `agent/` | C++17 | **Keep (core)** |
| Agent Studio (GUI) | `agent_studio/` | Dart/Flutter | **Keep, slim down** |
| Agent Server (web API) | `agent_server/` | Dart | **Delete** — replaced by C++ server |
| MCP tool server | `mcp-server/` | C++ | **Keep** (50+ tools, engine connects via `am_connect_mcp`) |
| MCP server (dup) | `mcp_server/` | Dart | **Delete** — duplicate |
| Auth server | `auth-server/` | C++ | Keep (out of scope here) |
| Data server | `data server/` | C++ | Keep (out of scope here) |

### The core problem: LLM calls are implemented **three times**
1. ✅ **Engine (C++)** — `agent/src/agent/llm_factory.cpp` +
   `ai_model_llm_client.cpp` (`AIModelLLMClient` → `AIModel`). This is the one we
   want. Supports `openai, anthropic, google, ollama, groq, mistral, deepseek,
   xai, openrouter, together, lmstudio, llamacpp, vllm, llama, custom, mock`.
2. ❌ **Dart server** — `agent_server/lib/llm_router.dart` +
   `agent_loop.dart` re-implement the loop and call providers directly.
3. ❌ **Flutter app** — `agent_studio/lib/services/llm_service.dart` re-implements
   provider calls again (this is what currently powers chat + benchmarking).

The Dart `agent_server` keeps its own in-memory `agent_store` and never invokes
the engine — so the web app today is effectively a *different product* from the
FFI/desktop path.

### What already exists and will be reused (important)
The engine's C API (`agent/src/c_api/c_api.cpp`) already exposes everything the
features need:

| C API symbol | Powers |
|---|---|
| `am_create` / `am_destroy` | Engine lifecycle (server boot / app start) |
| `am_configure_llm` | Set provider+model+key → AiModel (replaces all Dart LLM) |
| `am_spawn_agent` / `am_destroy_agent` / `am_list_agents` | Agent CRUD |
| `am_run_agent` + `am_future_wait` | Run the agent loop |
| `am_inject_work` | **Manipulate / override an agent's input prompt** |
| **`am_pipe(from_id, to_id)`** | **Redirect one agent's output → another's input** |
| `am_send_message` / `am_broadcast` | Agent-to-agent messaging |
| `am_blackboard_read/write/keys` | Shared state between agents |
| `am_fan_out` / `am_research_from_angles` | Parallel formations |
| `am_subscribe_events` (+ `am_event_cb`) | **Live events for the GUI + benchmark metrics** |
| `am_connect_mcp` / `am_disconnect_mcp` / `am_list_mcp_servers` | Tools via C++ mcp-server |

The Flutter FFI bindings (`agent_studio/lib/ffi/agent_engine_bindings.dart`)
already bind `am_create, am_spawn_agent, am_run_agent, am_pipe, am_send_message,
am_broadcast, am_get_status, am_list_agents, …`. **Local mode is ~80% wired.**
The Flutter `AgentBackend` interface
(`agent_studio/lib/services/agent_api_service.dart`) already abstracts
FFI vs HTTP — we extend it, we don't rewrite it.

---

## 3. Target architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    Flutter app  (GUI ONLY)                     │
│   screens / widgets / providers  —  NO llm_service.dart        │
│                                                                │
│   AgentBackend (abstract)                                      │
│     ├── FfiBackend   (desktop/local)  ──FFI──┐                 │
│     └── HttpBackend  (web)  ──HTTP + WS──┐    │                │
└──────────────────────────────────────────┼────┼──────────────┘
                                            │    │
                              ┌─────────────▼┐   │
                              │  C++ server  │   │   (same libagent_engine)
                              │ (embeds eng) │   │
                              └──────┬───────┘   │
                                     │           │
                              ┌──────▼───────────▼──────┐
                              │   Agent Engine  (C++)    │
                              │  agent loop + AiModel    │──► Claude / GPT / Gemini / Ollama …
                              │  llm_factory             │
                              └──────────┬───────────────┘
                                         │ am_connect_mcp
                                  ┌──────▼───────┐
                                  │ mcp-server   │ (C++ tools)
                                  └──────────────┘
```

Responsibilities:
- **Flutter** = rendering, gestures, local state, calling `AgentBackend`. Nothing else.
- **C++ server** = thin HTTP/WS adapter over `agent::AgentManager`. Holds one
  engine instance; translates requests → C API; streams engine events → WS.
- **Engine** = the loop, all LLM, benchmark instrumentation, tools.

---

## 4. The single server (new, C++)

**Location:** new `agent_server_cpp/` (replaces Dart `agent_server/`). Reuses
`cpp-httplib` (already used by `mcp-server/`), links `libagent_engine`.

**Why C++ (not a thin Dart proxy):** the engine is C++; embedding it in-process
avoids a second language runtime, a second LLM path, and FFI-from-a-server
complexity. One binary owns the engine.

**Boots:** `am_create({...})`, optional `am_configure_llm`, then `listen(host,port)`.

### HTTP API (REST) — keeps the existing path shapes so the GUI barely changes
| Method & path | Engine call | Notes |
|---|---|---|
| `GET /health` | — | `{status, version, api_version}` |
| `POST /api/llm/configure` | `am_configure_llm` | provider/model/api_key/base_url |
| `GET /api/agents` | `am_list_agents` | |
| `POST /api/agents` | `am_spawn_agent` | returns id |
| `GET /api/agents/<id>` | `am_get_status` | |
| `DELETE /api/agents/<id>` | `am_destroy_agent` | |
| `POST /api/agents/<id>/run` | `am_run_agent`+`am_future_wait` | `{prompt}` |
| `POST /api/agents/<id>/inject` | `am_inject_work` | **override input prompt** |
| `POST /api/agents/<id>/cancel` | `am_cancel_agent` | |
| `POST /api/pipe` | **`am_pipe`** | `{from, to}` — the connector line |
| `DELETE /api/pipe` | (engine: unpipe) | remove a connector |
| `POST /api/agents/<from>/send` | `am_send_message` | `{to, message}` |
| `POST /api/broadcast` | `am_broadcast` | `{from, message}` |
| `GET/POST /api/blackboard` | `am_blackboard_*` | |
| `POST /api/mcp` / `DELETE /api/mcp/<name>` / `GET /api/mcp` | `am_connect_mcp` … | |
| `POST /api/benchmark` | runs + aggregates | see §7 |

### Live events (GUI status + benchmark metrics)
Engine already pushes events via `am_subscribe_events`. The server forwards them
to clients. **Transport decision (recommended): WebSocket** at `GET /events`
(the Flutter app already depends on `web_socket_channel`). SSE is the fallback if
we want to avoid a WS dependency in the C++ server.

Event payloads (already emitted by the engine): `agent_started`,
`work_item_started`, `agent_finished`, `agent_failed`, `agent_cancelled`,
`blackboard_updated`, `mcp_connected/disconnected`, `quota_exceeded`. We add
benchmark-related fields (tokens, latency) to `agent_finished` (see §7).

---

## 5. Flutter app: become GUI-only

### Delete
- `agent_studio/lib/services/llm_service.dart` (and its `LlmResult`).
- `agent_studio/lib/services/model_service.dart`'s **direct** provider calls →
  model lists come from the engine instead (`/api/llm/models` or static presets
  passed to `am_configure_llm`).

### Change
- `AgentProvider.sendMessage()` / `runBenchmark()` stop calling `_llmService`;
  they call `AgentApiService` (→ FFI or HTTP). The chat reply and benchmark
  metrics come back from the engine.
- `AgentProvider.setAgentChain()` → calls `am_pipe` (it already models the
  `chainToId` edge; now it's enforced by the engine, not Dart recursion).
- Web backend (`agent_api_service.dart` HttpBackend) gains: `pipe`, `inject`,
  `send`, `broadcast`, `benchmark`, and a WS `engineEvents` stream.
- FFI backend (`engine_service_ffi.dart`) gains the same methods (bindings for
  `am_pipe`, `am_inject_work`, `am_send_message`, `am_broadcast` already exist).

### Keep
- All screens/widgets, theming, the benchmark **UI** I just built (it gets fed by
  engine metrics instead of in-app LLM calls), providers, models.

---

## 6. Feature: draw-to-connect formation editor

A new canvas screen (`agent_studio/lib/screens/formation_canvas.dart`) replacing
the edge-map editor in `group_builder_dialog.dart` for graph formations.

- **Nodes** = agent cards placed on a pannable/zoomable canvas; positions saved
  in `AgentModel.metadata['pos']`.
- **Ports** = each node has an **output port (right)** and **input port (left)**.
- **Draw a connector:** press an output port → drag → release on another node's
  input port. On release: create the edge, call `backend.pipe(from, to)`, and
  add to the group's `edges` map.
- **Rendering:** `CustomPainter` draws bezier connectors; arrowheads show
  direction (output→input). Live "flowing" animation while a pipe is active
  (driven by WS events).
- **Editing:** tap a connector to select; delete key / button removes it
  (`DELETE /api/pipe`). Cycle detection warns before creating a loop.
- **Run:** a "Run formation" button seeds the source node(s) with the prompt;
  the engine executes the wired graph (`am_pipe` chains + `am_fan_out` for
  parallel branches).

This directly satisfies "connect agents by drawing a connector line" and
"redirect their output to other agents."

---

## 7. Feature: benchmark agents on **all** parameters

### Metrics (all four groups, confirmed)
| Group | Metrics | Source |
|---|---|---|
| **Speed** | total latency, time-to-first-token, output tokens/sec | engine timing around `complete()` |
| **Cost & tokens** | prompt / completion / total tokens, est. $ cost | provider usage + a per-model price table |
| **Quality** | LLM-as-judge score (0–10), expected-answer match, similarity | optional judge agent + test-case expected output |
| **Reliability** | success rate, error type, retries, loop iterations, tool-call count, cancellations | engine loop counters + events |

### Where it runs
**In the engine**, not the app. Add a lightweight metrics struct to the agent
context, populated during the loop, and:
1. Return it in the `am_run_agent` result JSON, **and**
2. Emit it on the `agent_finished` event (so the UI updates live).

Add `POST /api/benchmark { prompt, targets: [agentId|{provider,model}], params,
repeats, expected? }` that runs each target (concurrently), collects metrics, and
returns a comparison table. The benchmark **UI already exists** (leaderboard,
per-run output, tok/s) — it switches from reading `LlmResult` to reading engine
metrics, and gains columns for cost, quality, and reliability.

### Quality scoring options (pick during Phase 4)
- **Expected-answer match:** test cases with a known answer → exact/regex/embedding similarity.
- **LLM-as-judge:** a judge agent (any model) scores each output 0–10 with a rubric.
Both run through the engine like any other agent.

---

## 8. Migration phases (each phase is shippable & reviewable)

**Phase 0 — this document.** ✅ Approve the architecture.

**Phase 1 — Stand up the C++ server (no deletions yet).**
- New `agent_server_cpp/` linking `libagent_engine` + `cpp-httplib`.
- Implement REST endpoints in §4 + WS `/events`.
- `docker-compose.yml`: `api` service builds the C++ server instead of Dart.
- Acceptance: `curl /health`, spawn+run an agent end-to-end through the engine.

**Phase 2 — Point Flutter web at the C++ server; remove Flutter LLM.**
- Extend `HttpBackend` with pipe/inject/send/broadcast/benchmark + WS events.
- Delete `llm_service.dart`; route chat + benchmark through `AgentApiService`.
- Acceptance: web app chats and benchmarks with **zero** direct provider calls
  (verify via network inspector — only traffic is to our server).

**Phase 3 — Delete the duplicates.**
- Delete `agent_server/` (Dart) and `mcp_server/` (Dart).
- Update `setup/`, `start.sh`, `deploy.sh`, `docker-compose.yml`, `Dockerfile.web`.
- Acceptance: full stack builds & runs from `docker compose up` with only the
  C++ server + C++ mcp-server + Flutter web.

**Phase 4 — Engine benchmark instrumentation + benchmark API.**
- Add metrics struct + counters in the loop; emit on `agent_finished`.
- `POST /api/benchmark`; wire the existing benchmark UI to engine metrics; add
  cost/quality/reliability columns. Add price table + judge/expected-answer.

**Phase 5 — Draw-to-connect formation canvas.**
- New canvas screen, ports, bezier connectors, `am_pipe` on connect, live flow
  animation from WS events, cycle detection, run-formation.

**Phase 6 — Local/desktop parity & polish.**
- Fill any missing FFI methods in `engine_service_ffi.dart`; ensure the canvas,
  chat, and benchmark all work identically over FFI and HTTP.

---

## 9. Risks & mitigations
- **WebSocket in C++ httplib** is limited → use a small WS lib (e.g.
  websocketpp/uWebSockets) or fall back to SSE. *Decide in Phase 1.*
- **FFI threading:** `am_subscribe_events` callbacks fire on engine threads →
  marshal to the Flutter UI isolate via a `ReceivePort`. *Phase 6.*
- **Web can't FFI:** confirmed handled — `agent_api_service.dart` already
  conditional-imports FFI vs stub; web uses HTTP only.
- **Token usage availability:** not every provider returns usage; benchmark
  shows "—" and estimates from char counts when missing.
- **Secrets:** API keys go to the **engine/server**, never shipped in web JS.
  Keys are entered in the GUI and `POST`ed to `/api/llm/configure` over the
  server connection (use TLS in deploy).

## 10. Open decisions (need answers before Phase 1/4)
1. **Event transport:** WebSocket (recommended) vs SSE for `/events`?
2. **Where do model lists come from** once `model_service.dart` is gone — a new
   engine endpoint `GET /api/llm/models`, or static presets per provider?
3. **Quality scoring default:** LLM-as-judge, expected-answer, or both?
4. **Auth:** does the C++ server sit behind `auth-server` like `mcp-server` does?
