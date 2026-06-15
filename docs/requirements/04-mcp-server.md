# 04 — MCP Tool Server (C++)  `mcp-server/`

**Action:** Keep. Minor/optional changes. This is the surviving tool server; the
engine connects to it via `am_connect_mcp`. The Dart `mcp_server/` is deleted
(doc 05).

## Overview
C++ server (`mcp-server/src/`, uses cpp-httplib) that runs 50+ tools implemented
as Python scripts in `mcp-server/tools/` (`web_search`, `weather_*`, `github_*`,
`maps_*`, `wikipedia_*`, `wolfram_*`, `youtube_*`, `finance_*`, `util_*`, …).
`tool_runner` feeds each script JSON on stdin (`{"args":…, "api_keys":…}`) and
`auth.cpp` validates bearer tokens against `auth-server`'s `/introspect`.

## In scope
- Confirm it is the single tool server.
- Ensure the engine can connect and call tools end-to-end.
- Document the tool manifest + key handling.

## Out of scope
- Agent loop / LLM (engine). Replacing the tool transport.

## Functional requirements
- **MCP-1 [MUST]** Remains the only MCP server (Dart `mcp_server/` removed).
- **MCP-2 [MUST]** Engine connects via `am_connect_mcp({name,url,bearer_token,
  transport})` and lists/uses tools from `tools_manifest.json`.
- **MCP-3 [MUST]** Each tool runs via `tool_runner` (stdin JSON →
  `{"args":…, "api_keys":…}`); the server injects `api_keys` from its environment,
  not from the client.
- **MCP-4 [SHOULD]** Auth: requests validated through `auth.cpp` →
  `AUTH_INTROSPECT_URL`; audience/scope enforced (align with G-5).
- **MCP-5 [SHOULD]** `tools_manifest.json` stays the source of truth for available
  tools (name, description, args schema, required api_keys).
- **MCP-6 [MAY]** Expose a health endpoint for `docker-compose` readiness.

## Non-functional
- **MCP-7 [MUST]** Tool execution is sandboxed to the script process; a failing
  tool returns an error, never crashes the server.
- **MCP-8 [SHOULD]** Per-tool timeout; concurrent tool calls supported.
- **MCP-9 [MUST]** API keys never returned to clients or logged.

## Build & dependencies
- **MCP-10 [MUST]** C++17 + cpp-httplib; Python 3 runtime available in the
  container for `tools/`. `CMakeLists.txt` + Dockerfile + compose service.

## Acceptance criteria
- [ ] Engine connects to `mcp-server` and an agent successfully calls e.g.
      `web_search` end-to-end.
- [ ] Missing/invalid bearer token is rejected (if G-5 enforces auth).

## Decisions to confirm (edit me)
- G-5 auth enforcement on/off for local dev.
- Which tools require which API keys (review `tools_manifest.json`).

## Change log
- _draft_ — initial requirements.
