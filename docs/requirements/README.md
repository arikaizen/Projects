# Agent Studio — Requirements

Per-component requirement specs for the single-engine consolidation described in
[`/ARCHITECTURE_PLAN.md`](../../ARCHITECTURE_PLAN.md). **These are drafts for you
to edit.**

## Documents
| # | Component | Path | Action |
|---|---|---|---|
| 01 | Agent Engine (C++) | [`01-agent-engine.md`](01-agent-engine.md) | Extend |
| 02 | Engine Server (C++, new) | [`02-cpp-server.md`](02-cpp-server.md) | Build new |
| 03 | Agent Studio (Flutter GUI) | [`03-flutter-app.md`](03-flutter-app.md) | Slim to GUI-only |
| 04 | MCP Tool Server (C++) | [`04-mcp-server.md`](04-mcp-server.md) | Keep / minor |
| 05 | Decommission (Dart) | [`05-decommission-dart.md`](05-decommission-dart.md) | Delete |

## Conventions
- **Requirement IDs**: `PREFIX-n` (e.g. `ENG-3`, `SRV-7`). Stable — don't renumber;
  retire with ~~strikethrough~~ instead.
- **Priority**: `[MUST]` / `[SHOULD]` / `[MAY]` (RFC-2119 sense).
- **Status**: ✅ done · 🟡 in progress · ⬜ not started · ❌ won't do.
- Prefixes: `ENG` engine · `SRV` server · `APP` Flutter · `MCP` tools · `DEC` decommission.

## Global decisions (affect multiple components — confirm first)
| ID | Decision | Options | Default (recommended) | Your call |
|---|---|---|---|---|
| G-1 | Event transport | WebSocket / SSE | **WebSocket** | _edit_ |
| G-2 | Engine HTTPS dep | OpenSSL required for cloud models | **Yes, require OpenSSL** | _edit_ |
| G-3 | Model list source | engine endpoint / static presets | **engine endpoint `GET /api/llm/models`** | _edit_ |
| G-4 | Quality scoring | LLM-judge / expected-answer / both | **Both** | _edit_ |
| G-5 | Server auth | behind `auth-server` / open / token | **behind auth-server** | _edit_ |
