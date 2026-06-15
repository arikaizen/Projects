# 02 — Engine Server (C++, new)  `agent_server_cpp/`

**Action:** Build new. The **single** server. Replaces the Dart `agent_server/`
(doc 05). Embeds the engine in-process and serves the Flutter **web** app.

## Overview
A thin C++ HTTP + live-event adapter over `agent::AgentManager`. It owns one
engine instance, translates each request into a C API / `AgentManager` call, and
streams engine events to connected clients. It performs **no** LLM logic of its
own — all model traffic happens inside the engine (doc 01).

## In scope
- HTTP REST endpoints mirroring today's path shapes so the Flutter client changes
  minimally.
- A live event channel (WebSocket, per G-1).
- Boot/config, health, optional auth.

## Out of scope
- LLM calls (engine only). Tool execution (mcp-server, doc 04).

## Functional requirements

### Lifecycle & config
- **SRV-1 [MUST]** On boot: create the engine (`AgentManager` / `am_create`),
  read host/port from env (`HOST`, `PORT`, default `0.0.0.0:3001`), then listen.
- **SRV-2 [MUST]** `POST /api/llm/configure` → `am_configure_llm`. Body:
  `{provider, model, api_key, base_url?}`.
- **SRV-3 [SHOULD]** `GET /api/llm/models` → engine model list (G-3).

### Agent endpoints (map 1:1 to C API)
- **SRV-4 [MUST]** `GET /api/agents` → `am_list_agents`.
- **SRV-5 [MUST]** `POST /api/agents` → `am_spawn_agent`; returns `{id}`.
- **SRV-6 [MUST]** `GET /api/agents/<id>` → `am_get_status`.
- **SRV-7 [MUST]** `DELETE /api/agents/<id>` → `am_destroy_agent`.
- **SRV-8 [MUST]** `POST /api/agents/<id>/run` → `am_run_agent` + `am_future_wait`;
  body `{prompt}`; returns result JSON **incl. benchmark metrics** (ENG-16).
- **SRV-9 [MUST]** `POST /api/agents/<id>/inject` → `am_inject_work` (edit input).
- **SRV-10 [MUST]** `POST /api/agents/<id>/cancel` → `am_cancel_agent`.

### Wiring / formations
- **SRV-11 [MUST]** `POST /api/pipe` `{from,to}` → `am_pipe` (draw a connector).
- **SRV-12 [MUST]** `DELETE /api/pipe` `{from,to}` → `am_unpipe` (ENG-9).
- **SRV-13 [SHOULD]** `POST /api/agents/<from>/send` `{to,message}` → `am_send_message`.
- **SRV-14 [SHOULD]** `POST /api/broadcast` `{from,message}` → `am_broadcast`.
- **SRV-15 [SHOULD]** `GET/POST /api/blackboard` → `am_blackboard_*`.
- **SRV-16 [MAY]** `POST /api/fanout` → `am_fan_out` for parallel branches.

### Tools (MCP)
- **SRV-17 [SHOULD]** `POST /api/mcp`, `DELETE /api/mcp/<name>`, `GET /api/mcp` →
  `am_connect_mcp` / `am_disconnect_mcp` / `am_list_mcp_servers`.

### Benchmarking
- **SRV-18 [MUST]** `POST /api/benchmark` body
  `{prompt, targets:[agentId | {provider,model}], params, repeats?, expected?}`.
  Runs targets concurrently through the engine, aggregates metrics (ENG-13),
  returns a comparison array. Quality per G-4.

### Live events
- **SRV-19 [MUST]** `GET /events` (WebSocket, G-1): on connect, `am_subscribe_events`;
  forward every event as JSON. Includes `agent_finished` with metrics for live
  benchmark/status updates. Multiple clients supported (fan-out).
- **SRV-20 [MUST]** Marshal engine-thread callbacks onto the send path safely.

### Health & errors
- **SRV-21 [MUST]** `GET /health` → `{status:"ok", server, version, api_version}`.
- **SRV-22 [MUST]** Map `am_status_t` → HTTP codes; error body `{error, detail}`
  from `am_last_error`.
- **SRV-23 [SHOULD]** CORS configured for the web origin.

## Non-functional
- **SRV-24 [MUST]** Concurrency: thread-pooled request handling (httplib default
  is fine); engine is shared (see ENG-20).
- **SRV-25 [SHOULD]** Graceful shutdown (`am_destroy` on SIGINT/SIGTERM).
- **SRV-26 [SHOULD]** Structured request logging; never log api keys/prompts at
  default level.

## Build & dependencies
- **SRV-27 [MUST]** C++17, links `libagent_engine` (built per ENG-14), cpp-httplib
  (+ a WS lib if G-1=WebSocket, else SSE over httplib chunked responses), OpenSSL,
  nlohmann/json. `CMakeLists.txt` + `Dockerfile`.
- **SRV-28 [MUST]** `docker-compose.yml` `api` service builds this instead of Dart.

## Acceptance criteria
- [ ] `docker compose up` serves `/health`; spawn→run an agent end-to-end through
      the engine (real model reply).
- [ ] `/api/pipe` then run produces a chained result; `/events` streams
      `agent_started/finished` to a connected web client.
- [ ] `/api/benchmark` returns metrics for ≥2 models.

## Decisions to confirm (edit me)
- G-1 WebSocket vs SSE. · G-5 auth (behind `auth-server`?). · Port (default 3001).
- Final directory name: `agent_server_cpp/`?

## Change log
- _draft_ — initial requirements.
