# MCP Server

A Model Context Protocol (MCP) server written in C++17 that bridges an LLM host (Claude Desktop, Claude Code) to the graph database server over TCP. The LLM calls tools; this server translates them into graph DB commands and returns the results.

---

## How It Works

```
LLM Host (Claude)
      │  JSON-RPC 2.0 on stdin/stdout
      ▼
 mcp_server  ←── this project
      │  line-based text protocol over TCP
      ▼
 graph DB server  (port 7474)
```

The LLM host spawns `mcp_server` as a child process and communicates over its stdin/stdout using newline-delimited JSON-RPC 2.0. The MCP server maintains a persistent TCP connection to the graph DB and forwards each tool call as a single command line, then returns the response.

---

## Building

```bash
cd "mcp server"
make
```

Requires a C++17-capable compiler (`g++` or `clang++`) and POSIX sockets. No third-party libraries.

```bash
make CXX=clang++   # build with clang
make clean         # remove build/ and the binary
```

The binary is placed at `mcp server/mcp_server`.

---

## Running

Start the graph DB server first, then run the MCP server:

```bash
./mcp_server --host 127.0.0.1 --port 7474
```

| Flag | Default | Description |
|------|---------|-------------|
| `--host` | `127.0.0.1` | Hostname or IPv4 address of the graph DB server |
| `--port` | `7474` | TCP port the graph DB server is listening on |

The server exits immediately with a clear error message if it cannot connect to the graph DB.

### Claude Desktop Configuration

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "my-graph-db": {
      "command": "/path/to/mcp server/mcp_server",
      "args": ["--host", "127.0.0.1", "--port", "7474"]
    }
  }
}
```

---

## Tools

The server exposes nine tools that map directly to the graph DB's nine commands.

### Entity Operations

| Tool | Description | Arguments |
|------|-------------|-----------|
| `create_entity` | Create a new named entity in the knowledge graph | `name` (string, required), `type` (string, required) |
| `add_observation` | Append a free-text fact to an existing entity | `entity` (string, required), `observation` (string, required) |
| `delete_entity` | Delete an entity and all its relations (cascade) | `name` (string, required) |
| `delete_observation` | Remove one specific observation (exact text match) | `entity` (string, required), `observation` (string, required) |

### Relation Operations

| Tool | Description | Arguments |
|------|-------------|-----------|
| `create_relation` | Create a directed, labelled edge between two entities | `from` (string, required), `relationType` (string, required), `to` (string, required) |
| `delete_relation` | Remove the edge identified by the exact (from, relationType, to) triple | `from` (string, required), `relationType` (string, required), `to` (string, required) |

### Query Operations

| Tool | Description | Arguments |
|------|-------------|-----------|
| `search_nodes` | Case-insensitive substring search across name, type, and observations | `query` (string, required) |
| `get_relations` | Get all edges where the entity appears as source or target | `entity` (string, required) |
| `read_graph` | Dump the complete graph — all entities and all relations | *(none)* |

---

## Wire Protocol

### MCP Side (stdin/stdout)

The MCP server speaks [JSON-RPC 2.0](https://www.jsonrpc.org/specification) with newline framing: exactly one JSON object per line.

**Request (from LLM host):**
```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"create_entity","arguments":{"name":"Alice","type":"person"}}}
```

**Response (to LLM host):**
```json
{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text","text":"{}"}]}}
```

**Tool error (application-level, not a protocol error):**
```json
{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text","text":"Entity already exists: Alice"}],"isError":true}}
```

Supported methods:

| Method | Description |
|--------|-------------|
| `initialize` | MCP handshake — returns server capabilities and protocol version |
| `notifications/initialized` | Client notification (no response sent) |
| `tools/list` | Returns all nine tool schemas in JSON Schema format |
| `tools/call` | Executes a tool and returns the result |
| `ping` | Heartbeat — returns `{}` |

### Graph DB Side (TCP)

Commands are single text lines terminated by `\n`. Arguments are separated by ` | ` (space-pipe-space).

| MCP Tool | Graph DB Command |
|----------|-----------------|
| `create_entity` | `CREATE_ENTITY {name} \| {type}` |
| `add_observation` | `ADD_OBS {entity} \| {observation}` |
| `create_relation` | `CREATE_REL {from} \| {relationType} \| {to}` |
| `delete_entity` | `DELETE_ENTITY {name}` |
| `delete_observation` | `DELETE_OBS {entity} \| {observation}` |
| `delete_relation` | `DELETE_REL {from} \| {relationType} \| {to}` |
| `search_nodes` | `SEARCH {query}` |
| `get_relations` | `GET_RELATIONS {entity}` |
| `read_graph` | `READ_GRAPH` |

Responses are single lines prefixed with `OK ` (success, followed by a JSON payload) or `ERROR ` (failure, followed by a message).

---

## Project Structure

```
mcp server/
├── mcp_types/
│   ├── mcp_types.h       McpRequest, McpResponse, ToolCall structs
│   └── mcp_types.cpp
├── json_rpc/
│   ├── json_rpc.h        JSON-RPC 2.0 parser and formatter (no third-party libs)
│   └── json_rpc.cpp      Recursive-descent parser with full \uXXXX → UTF-8 decoding
├── tool_registry/
│   ├── tool_registry.h   Tool schema definitions and validation
│   └── tool_registry.cpp Registers all 9 tools; builds tools/list JSON payload
├── db_client/
│   ├── db_client.h       TCP client interface + translation table
│   └── db_client.cpp     Persistent socket; handles TCP stream fragmentation
├── mcp_handler/
│   ├── mcp_handler.h     Method dispatcher interface
│   └── mcp_handler.cpp   Dispatches initialize / tools/list / tools/call / ping
├── server/
│   ├── mcp_server.h      stdin/stdout event loop interface
│   └── mcp_server.cpp    Reads lines, parses, dispatches, writes + flushes stdout
├── main.cpp              Entry point — arg parsing, connect, construct, run
├── Makefile              Component-per-folder build with build/ object directory
└── .gitignore
```

---

## Architecture Notes

### stdout Purity
`stdout` is reserved exclusively for MCP JSON-RPC messages. A single stray byte will corrupt the protocol stream. All logging and debug output goes to `stderr`.

### Mandatory Flush
`std::cout.flush()` is called after every response. The LLM host's blocking read will hang indefinitely if the C++ stream buffer is not flushed.

### Notification Suppression
JSON-RPC notifications (`id` absent or `null`) must not receive a response. The event loop detects `id == "null"` and skips the write step while still passing the message to the handler for processing.

### Tool Errors vs Protocol Errors
- **Protocol errors** (unknown method, bad params) go in the JSON-RPC `error` field.  
- **Graph DB errors** (e.g. entity not found) go in the `result` field as `{"content":[...],"isError":true}` so the LLM sees the message and can reason about it.

### TCP Partial Reads
`DbClient` maintains a persistent `m_recvBuffer` across `send()` calls. Because TCP is a stream protocol, a single `recv()` may return part of a line; the buffer accumulates bytes until a `\n` is found.

### JSON Parser
The parser is hand-written (no third-party libraries). It implements a minimal recursive-descent approach that handles the specific shapes produced by well-behaved MCP hosts: strings with full escape decoding (including `\uXXXX` → UTF-8), nested objects, depth-tracked array/object skipping, and numbers/booleans/null as raw substrings.
