# 03 — Agent Studio (Flutter GUI)  `agent_studio/`

**Action:** Slim to **GUI-only**. The app runs the engine two ways — FFI (local
desktop) and HTTP/WS (web) — through one `AgentBackend` abstraction. It must make
**no** direct LLM provider calls.

## Overview
Flutter front end (Provider state management, dark theme). Today it has a third
duplicate LLM path in `lib/services/llm_service.dart` plus `model_service.dart`
provider calls; both go away. UI talks only to `AgentApiService` →
`engine_service_ffi.dart` (desktop, FFI bindings already exist) or the HTTP
backend (web).

## In scope
- Remove in-app LLM; route chat + benchmark through the backend.
- Extend both backends (FFI + HTTP) with the full engine surface.
- New draw-to-connect formation canvas.
- Benchmark UI fed by engine metrics (UI already built).

## Out of scope
- Any model API calls. Engine internals.

## Functional requirements

### Become GUI-only
- **APP-1 [MUST]** Delete `lib/services/llm_service.dart` (and `LlmResult`).
- **APP-2 [MUST]** Remove direct provider HTTP from `lib/services/model_service.dart`;
  model lists come from the engine (G-3) or static presets.
- **APP-3 [MUST]** `AgentProvider.sendMessage()` calls the backend, not
  `_llmService`. Chat reply comes from the engine run result.
- **APP-4 [MUST]** `AgentProvider.runBenchmark()` calls `POST /api/benchmark`
  (web) / FFI equivalent; metrics come from the engine.
- **APP-5 [MUST]** No `package:http` call in the app targets a model provider
  domain. (Verifiable via grep + web network inspector.)

### Backend abstraction (extend existing `AgentBackend`)
- **APP-6 [MUST]** Add to the `AgentBackend` interface and both impls:
  `pipe(from,to)`, `unpipe(from,to)`, `injectWork(id, work)`,
  `sendMessage(from,to,msg)`, `broadcast(from,msg)`,
  `runBenchmark(request)`, `configureLlm(config)`, `listModels()`.
- **APP-7 [MUST]** `engineEvents` is a live `Stream<Map>` in **both** backends:
  - HTTP: WebSocket to `/events` (G-1) via `web_socket_channel` (already a dep).
  - FFI: `am_subscribe_events` → marshal to UI isolate via `ReceivePort`.
- **APP-8 [MUST]** Startup checks `am_api_version()` (FFI) / `/health` `api_version`
  (HTTP) for compatibility.
- **APP-9 [SHOULD]** A single "Connection" setting chooses Local(FFI) vs
  Server(URL); the rest of the UI is identical.

### Agent control UI
- **APP-10 [MUST]** Create/edit/delete agents; set provider+model (→ `configureLlm`
  / per-agent llm), system prompt, temperature, tools.
- **APP-11 [MUST]** **Edit/override an agent's input prompt before a run**
  (→ `injectWork`).
- **APP-12 [MUST]** Live status from `engineEvents` (idle/running/done/error) on
  cards + tree.

### Draw-to-connect formation canvas (new)
- **APP-13 [MUST]** New screen `lib/screens/formation_canvas.dart`: pannable/
  zoomable canvas; agents are draggable nodes; positions persisted in
  `AgentModel.metadata['pos']`.
- **APP-14 [MUST]** Each node shows an **output port (right)** and **input port
  (left)**. Press output → drag → release on another node's input creates a
  connector and calls `backend.pipe(from,to)`.
- **APP-15 [MUST]** Connectors drawn as directional bezier curves (`CustomPainter`)
  with arrowheads (output→input).
- **APP-16 [MUST]** Select a connector and delete it → `backend.unpipe`.
- **APP-17 [SHOULD]** Cycle detection warns before creating a loop.
- **APP-18 [SHOULD]** "Run formation": seed source node(s) with a prompt; animate
  active connectors using `engineEvents`.
- **APP-19 [MAY]** Replace the edge-map editor in `group_builder_dialog.dart` with
  this canvas for `graph`/`pipeline` formations.

### Benchmark UI (already built — re-wire)
- **APP-20 [MUST]** Keep the Benchmark tab/leaderboard; feed it engine metrics.
- **APP-21 [SHOULD]** Add columns/cards for **cost, quality, reliability** (today
  it shows speed + tokens). Show "—" when a metric is unavailable.
- **APP-22 [MAY]** Benchmark against named agents (not just raw models) so a
  configured prompt/tools/temperature is part of the comparison.

## Non-functional
- **APP-23 [MUST]** `flutter build web` and a desktop target both compile.
- **APP-24 [MUST]** No API keys persisted in web local storage; keys are sent to
  the server's `/api/llm/configure` over the connection only.
- **APP-25 [SHOULD]** Match existing theme/conventions (`AppColors`, Provider).

## Acceptance criteria
- [ ] `grep` shows no provider domains in `lib/`; web network tab shows traffic
      only to our server.
- [ ] Chat works identically over FFI (desktop) and HTTP (web).
- [ ] Dragging a connector between two nodes pipes them; running produces a
      chained result; deleting the connector unpipes.
- [ ] Benchmark tab shows speed/cost/quality/reliability from the engine.

## Decisions to confirm (edit me)
- G-1 transport · G-3 model list source.
- APP-19: replace the old edge editor, or keep both?

## Change log
- _draft_ — initial requirements. (Note: benchmark UV/UI from PR #98 already
  exists and will be re-wired, not rebuilt.)
