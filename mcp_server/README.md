# Agent Studio MCP Server

A lightweight MCP (Model Context Protocol) JSON-RPC 2.0 server that bridges **Claude (Anthropic)** and **Ollama (local LLM)** for use with Agent Studio.

## Quick start

```bash
# Install dependencies
dart pub get

# Run with an Anthropic API key
ANTHROPIC_API_KEY=sk-ant-… dart run bin/server.dart

# Or pass the key directly
dart run bin/server.dart --anthropic-key sk-ant-…

# With a custom Ollama URL and port
dart run bin/server.dart --ollama-url http://192.168.1.10:11434 --port 4000
```

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/rpc` | MCP JSON-RPC 2.0 |
| `GET`  | `/health` | Health check + server info |
| `GET`  | `/tools` | List available MCP tools |

## MCP tools

### `chat`
Multi-turn conversation. Automatically routes `claude-*` models to Anthropic and everything else to Ollama.

```json
{
  "jsonrpc": "2.0", "id": 1,
  "method": "tools/call",
  "params": {
    "name": "chat",
    "arguments": {
      "model": "claude-sonnet-4-6",
      "system": "You are a helpful assistant.",
      "messages": [
        {"role": "user", "content": "Hello!"}
      ],
      "temperature": 0.7
    }
  }
}
```

### `complete`
Single-turn text completion.

```json
{
  "jsonrpc": "2.0", "id": 2,
  "method": "tools/call",
  "params": {
    "name": "complete",
    "arguments": {
      "prompt": "Summarise the history of AI in 3 sentences.",
      "model": "llama3:8b"
    }
  }
}
```

### `list_models`
Lists all available models across providers.

```json
{
  "jsonrpc": "2.0", "id": 3,
  "method": "tools/call",
  "params": { "name": "list_models", "arguments": {"provider": "all"} }
}
```

### `ping`
Health check with provider status.

## Connecting from Agent Studio

1. Open **Settings** in Agent Studio
2. Under **MCP Servers → Connect**, enter `http://localhost:3000`
3. The server will appear in the MCP servers list

## Provider routing

| Model prefix | Backend |
|---|---|
| `claude-*` | Anthropic API |
| `gpt-*`, `o1-*`, `o3-*` | OpenAI (not included) |
| Everything else (llama3, mistral, gemma…) | Ollama |

Override with `"provider": "anthropic"` or `"provider": "ollama"` in `chat` args.

## CLI flags

```
--port, -p         HTTP port (default: 3000)
--host, -H         Bind address (default: 127.0.0.1)
--anthropic-key    Anthropic API key (or use ANTHROPIC_API_KEY env)
--anthropic-url    Anthropic base URL (default: https://api.anthropic.com)
--ollama-url       Ollama base URL (default: http://localhost:11434)
--verbose, -v      Log every request/response to stderr
```
