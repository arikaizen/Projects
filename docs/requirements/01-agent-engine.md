# 01 — Agent Engine (C++)  `agent/`

**Action:** Extend the existing engine. It is the single source of truth for the
agent loop and all LLM calls.

## Overview
The C++ engine runs the agent loop (OODA-style stages under
`agent/src/agent/stages/`) and makes every LLM call through the `AIModel` class
(`agent/third_party/ai_model/aimodel_api.cpp`) selected by `llm_factory`
(`agent/src/agent/llm_factory.cpp`). It is consumed two ways: the C FFI
(`agent/src/c_api/c_api.cpp`) for the desktop app, and the new C++ server (doc 02)
for web. **Nothing else may call model providers.**

## In scope
- Confirm/finish hosted-API LLM connectivity (already present, build-gated).
- Add benchmark instrumentation (metrics) to the loop.
- Expose model listing + benchmark data through the C API.

## Out of scope
- HTTP/WS serving (that's the C++ server, doc 02).
- GUI concerns.

## Functional requirements

### LLM connectivity
- **ENG-1 [MUST] ✅** All model calls go through `AIModel` via `llm_factory`
  `makeLLMClientFromConfig({provider, model, api_key, base_url})`. Providers
  already supported: openai, anthropic, google, ollama, groq, mistral, deepseek,
  xai, openrouter, together, lmstudio, llamacpp, vllm, llama, custom, mock.
- **ENG-2 [MUST]** The engine **must build with `-DAGENT_ENABLE_API_LLM=ON`** (and
  OpenSSL) in all shipped builds so cloud models work over HTTPS. _(see ENG-14)_
- **ENG-3 [MUST]** `am_configure_llm(config_json)` sets the default backend at
  runtime; per-agent `"llm"` override in spawn config also honored.
- **ENG-4 [SHOULD]** API keys are accepted at runtime (not compiled in) and never
  logged.

### Agent control & wiring (already in C API — verify & document)
- **ENG-5 [MUST] ✅** Spawn/list/destroy/status: `am_spawn_agent`,
  `am_list_agents`, `am_destroy_agent`, `am_get_status`.
- **ENG-6 [MUST] ✅** Run loop: `am_run_agent` + `am_future_wait`; cancel:
  `am_cancel_agent`.
- **ENG-7 [MUST] ✅** **Manipulate input prompt:** `am_inject_work(agent_id, work)`.
- **ENG-8 [MUST] ✅** **Redirect output → input:** `am_pipe(from_id, to_id)`.
- **ENG-9 [SHOULD]** Add **`am_unpipe(from_id, to_id)`** to remove a connector
  (needed by the draw-to-connect editor's delete action). _(new)_
- **ENG-10 [MUST] ✅** Messaging & shared state: `am_send_message`, `am_broadcast`,
  `am_blackboard_read/write/keys`, `am_fan_out`, `am_research_from_angles`.
- **ENG-11 [MUST] ✅** Events: `am_subscribe_events(cb)` / `am_unsubscribe_events`.
- **ENG-12 [MUST] ✅** Tools: `am_connect_mcp`, `am_disconnect_mcp`,
  `am_list_mcp_servers` (→ doc 04).

### Benchmark instrumentation (new)
- **ENG-13 [MUST]** Per run, the loop records a metrics record covering **all
  parameter groups**:
  - *Speed:* `total_latency_ms`, `time_to_first_token_ms`, `output_tokens_per_sec`.
  - *Cost & tokens:* `prompt_tokens`, `completion_tokens`, `total_tokens`,
    `est_cost_usd` (from a per-model price table — see ENG-15).
  - *Reliability:* `success`, `error_type`, `retries`, `loop_iterations`,
    `tool_calls`, `cancelled`.
  - *Quality:* `quality_score` (0–10) and/or `expected_match` (bool) when a judge
    or expected answer is supplied (see G-4). May be filled by a judge agent.
- **ENG-15 [SHOULD]** Ship a small editable model→price table (USD per 1M
  in/out tokens) for cost estimation; unknown models report `est_cost_usd=null`.
- **ENG-16 [MUST]** Surface metrics two ways: (a) included in the `am_run_agent`
  result JSON, and (b) attached to the `agent_finished` event payload.
- **ENG-17 [SHOULD]** Add **`am_list_models(config_json, out)`** returning models
  the configured provider exposes (supports G-3) — or document the static-preset
  fallback if G-3 = presets.

### C API / ABI
- **ENG-18 [MUST]** Bump `am_api_version()` when the ABI changes; the Flutter FFI
  bindings (doc 03) check it at startup.
- **ENG-19 [MUST]** All new symbols are `extern "C"`, return `am_status_t`, and
  follow the existing JSON-in/JSON-out + `am_last_error` convention.

## Non-functional
- **ENG-20 [MUST]** Thread-safe: `AgentManager` is shared by server worker threads
  and FFI callers; event callbacks may fire on engine threads.
- **ENG-21 [SHOULD]** No exceptions cross the C API boundary (already the pattern).
- **ENG-22 [SHOULD]** Benchmark instrumentation adds < 1% overhead when idle.

## Build & dependencies
- **ENG-14 [MUST]** CMake: cpp-httplib (header-only) + OpenSSL for HTTPS;
  nlohmann/json; Threads. Document the exact configure line:
  `cmake -DAGENT_ENABLE_API_LLM=ON -DAGENT_ENABLE_MCP_HTTP=ON -DHTTPLIB_INCLUDE_DIR=...`
- Output: `libagent_engine` (shared, for FFI + server link).

## Acceptance criteria
- [ ] Built with `AGENT_ENABLE_API_LLM=ON`; an Anthropic + an OpenAI call each
      succeed over HTTPS via `am_configure_llm` + `am_run_agent`.
- [ ] `am_pipe` chains two agents end-to-end; `am_unpipe` removes the link.
- [ ] `am_run_agent` result and `agent_finished` event both carry the full metrics
      record (ENG-13).

## Decisions to confirm (edit me)
- G-2 OpenSSL requirement (default: yes).
- G-3 `am_list_models` vs static presets.
- ENG-15 price table — provide your own numbers?

## Change log
- _draft_ — initial requirements.
