# MCP Server

Model Context Protocol server implemented in **C++17** (`cpp-httplib`, `nlohmann/json`).  
Tool execution delegates to **Python scripts** — the C++ layer handles auth, JSON-RPC dispatch, and subprocess management.

## Architecture

```
LLM / Agent (Claude, etc.)
    │
    ③  POST /mcp/v1   (JSON-RPC 2.0 + Bearer token)
    │
C++ MCP Server (port 8081)
    │  ④  POST /introspect → Auth Server (port 8080)
    │
    │  ⑤  spawn python3 tools/<script>.py
    │        stdin  ← {"args": {...}, "api_keys": {...}}
    │        stdout → {"success": true/false, "result": ...}
    │
External APIs (Google, GitHub, OpenWeatherMap, ...)
```

**Key properties:**
- The C++ server handles all HTTP, Bearer-token validation, and JSON-RPC routing.
- Tool logic lives in Python scripts — easy to add, update, or swap.
- API credentials are **never forwarded to the LLM**; they are injected by the C++ server from its own environment variables at subprocess spawn time.

## Transport Modes

| Mode | Activation | Auth |
|------|-----------|------|
| **Streamable HTTP** | Default | Bearer token (validated via auth-server introspection) |
| **stdio** | `STDIN_STDIO=1` env var | None — subprocess trust model |

## Tool Categorization

Every tool in `tools/tools_manifest.json` carries a `"category"` field.  
The `tools/list` response includes the category on every entry so the model can filter efficiently:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}
```

Response includes `"category"` per tool. To list only a specific category, the model or agent can filter client-side or pass `{"category": "weather"}` in params (server filters server-side).

## Building

### Prerequisites

- CMake ≥ 3.17
- C++17 compiler
- OpenSSL ≥ 1.1.1
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) header-only
- Python 3.8+ (for tool scripts)
- `python3` in `PATH`

```bash
mkdir build && cd build
cmake .. -DHTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib
make -j$(nproc)
```

## Configuration

Copy `.env.example` to `.env`:

| Variable | Default | Description |
|----------|---------|-------------|
| `HOST` | `0.0.0.0` | Bind address |
| `PORT` | `8081` | Listen port |
| `AUTH_INTROSPECT_URL` | `http://localhost:8080/introspect` | Auth server introspection URL |
| `MCP_SERVER_AUDIENCE` | `http://localhost:8081` | Expected audience in Bearer tokens |
| `MCP_REQUIRED_SCOPE` | `tools:call` | Scopes required in every request |
| `TOOLS_DIR` | `../tools` | Directory containing Python scripts |
| `TOOLS_MANIFEST` | `../tools/tools_manifest.json` | Tool manifest path |

### API Keys

All API keys are read from environment variables and passed securely to each tool subprocess:

| Variable | Used by |
|----------|---------|
| `GOOGLE_API_KEY` | web_search, maps_*, translate_*, youtube_*, google_nlp_*, google_vision_* |
| `GOOGLE_CSE_ID` | web_search, web_search_images, web_search_news |
| `OPENWEATHER_API_KEY` | weather_* |
| `NEWSAPI_KEY` | news_* |
| `ALPHA_VANTAGE_KEY` | finance_* |
| `GITHUB_TOKEN` | github_* (optional — increases rate limits) |
| `DEEPL_API_KEY` | deepl_translate |
| `WOLFRAM_APP_ID` | wolfram_* |
| `YOUTUBE_API_KEY` | youtube_* (falls back to GOOGLE_API_KEY) |
| `UNSPLASH_ACCESS_KEY` | unsplash_* |
| `IPINFO_TOKEN` | util_ip_info (optional) |

## Tool Scripts Protocol

Each Python script in `tools/` follows this interface:

**stdin** (one JSON line):
```json
{"args": {"query": "example"}, "api_keys": {"GOOGLE_API_KEY": "AIza..."}}
```

**stdout** (one JSON line):
```json
{"success": true, "result": [...]}
```
or
```json
{"success": false, "error": "Descriptive error message"}
```

The `_base.py` helper handles `read_input()`, `success()`, and `failure()` boilerplate.

## JSON-RPC Methods

| Method | Description |
|--------|-------------|
| `ping` | Health check — returns `{"pong": true}` |
| `tools/list` | Returns all registered tools with names, descriptions, categories, and input schemas |
| `tools/call` | Execute a tool — params: `{"name": "tool_name", "arguments": {...}}` |

## Tools Reference

See [TOOLS.md](./TOOLS.md) for the full 55-tool catalogue.
