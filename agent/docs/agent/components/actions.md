# Built-in Actions

`src/agent/actions/`

All 13 built-in actions are subclasses of `Action` → `WorkItem`. They perform deterministic, side-effectful operations: filesystem I/O, shell execution, HTTP, messaging, memory, and MCP tool calls.

Actions are registered with `WorkFactory` via free `register*` functions called during `AgentManager` construction. Multiple actions with independent `$ref` deps execute in parallel on the `ThreadPool`.

---

## File System Actions

### BashAction

**Factory name:** `"BashAction"`  
**File:** `actions/bash_action.hpp` / `bash_action.cpp`

Runs a shell command via `popen()` and captures stdout and the exit code.

**Inputs:**
```json
{
  "command": "ls -la /tmp",
  "timeout_ms": 5000        // optional, default unlimited
}
```

**Output:**
```json
{"stdout": "...", "exit_code": 0}
```

**Thread safety:** `popen()` is thread-safe on glibc/Linux. On other platforms, serialize concurrent calls with an external mutex.

---

### ReadAction

**Factory name:** `"ReadAction"`  
**File:** `actions/read_action.hpp` / `read_action.cpp`

Reads a file from disk. Supports optional line range selection.

**Inputs:**
```json
{
  "path":   "/path/to/file.txt",
  "offset": 1,    // 1-based line start, optional
  "limit":  100   // max lines to return, optional
}
```

**Output:**
```json
{"content": "file contents...", "lines": 42}
```

**Thread safety:** Read-only filesystem access; fully concurrent.

---

### WriteAction

**Factory name:** `"WriteAction"`  
**File:** `actions/write_action.hpp` / `write_action.cpp`

Writes content to a file, creating parent directories as needed. Overwrites existing content.

**Inputs:**
```json
{
  "path":    "/path/to/output.txt",
  "content": "Hello, world!\n"
}
```

**Output:**
```json
{"path": "/path/to/output.txt", "bytes_written": 14}
```

**Thread safety:** Concurrent writes to the **same path** are unsafe. The caller (plan) must ensure only one `WriteAction` targets a given path at a time.

---

### EditAction

**Factory name:** `"EditAction"`  
**File:** `actions/edit_action.hpp` / `edit_action.cpp`

Performs an exact first-occurrence string replacement in a file. Fails if `old_string` is not found or is not unique.

**Inputs:**
```json
{
  "path":       "/path/to/file.cpp",
  "old_string": "int x = 0;",
  "new_string": "int x = 42;"
}
```

**Output:**
```json
{"replaced": true, "path": "/path/to/file.cpp"}
```

**Thread safety:** Concurrent edits to the **same file** are unsafe. Serialize via `$ref` dependencies.

---

### GlobAction

**Factory name:** `"GlobAction"`  
**File:** `actions/glob_action.hpp` / `glob_action.cpp`

Recursively searches a directory tree for paths matching a shell-style glob pattern. Supports `*` (any sequence within a path component) and `?` (any single character).

**Inputs:**
```json
{
  "pattern": "*.cpp",
  "root":    "./src"
}
```

**Output:**
```json
{"matches": ["src/foo.cpp", "src/bar.cpp"], "count": 2}
```

**Thread safety:** Read-only filesystem traversal; fully concurrent.

---

### GrepAction

**Factory name:** `"GrepAction"`  
**File:** `actions/grep_action.hpp` / `grep_action.cpp`

Searches file contents for a pattern. Tries `ripgrep` (`rg`) first for performance; falls back to a manual `std::ifstream` search if `rg` is not on PATH.

**Inputs:**
```json
{
  "pattern":   "TODO",
  "path":      "./src",
  "use_regex": false   // optional, default false
}
```

**Output:**
```json
{
  "matches": [
    {"file": "src/foo.cpp", "line": 42, "text": "// TODO: fix this"}
  ],
  "count": 1
}
```

**Thread safety:** Read-only; fully concurrent.

---

## Web Actions

### WebFetchAction

**Factory name:** `"WebFetchAction"`  
**File:** `actions/web_fetch_action.hpp` / `web_fetch_action.cpp`

Fetches a URL over HTTP/HTTPS. Uses `cpp-httplib` (`httplib.h`) if present at compile time; falls back to a `curl`-via-`popen` stub.

**Inputs:**
```json
{
  "url":     "https://api.example.com/data",
  "method":  "GET",           // optional, default "GET"
  "body":    "",              // optional request body
  "headers": {"Accept": "application/json"}  // optional
}
```

**Output:**
```json
{"status_code": 200, "body": "{...}", "headers": {}}
```

**Thread safety:** Each call is independent with no shared state; fully concurrent.

---

### WebSearchAction

**Factory name:** `"WebSearchAction"`  
**File:** `actions/web_search_action.hpp` / `web_search_action.cpp`

Stub web search action. As shipped, returns a placeholder response instructing the user to configure a search API key (SerpAPI, Brave Search, etc.). A real implementation would call the configured search API using a key from `AgentContext::config().extra`.

**Inputs:**
```json
{
  "query": "C++ concurrency best practices",
  "num_results": 5
}
```

**Output:**
```json
{"results": [], "note": "configure SEARCH_API_KEY to enable"}
```

---

## Agent Orchestration Actions

### TaskAction

**Factory name:** `"TaskAction"`  
**File:** `actions/task_action.hpp` / `task_action.cpp`

Spawns a sub-agent to execute a task and waits for its result (Pattern A delegation). Enforces `config.max_depth` to prevent infinite recursion.

**Inputs:**
```json
{
  "task":       "Summarise the file at /tmp/report.txt",
  "agent_name": "summariser",    // optional
  "max_depth":  2                // optional, defaults to parent's max_depth - 1
}
```

**Output:**
```json
{"result": {...}, "agent_id": "agent_3", "iterations": 4}
```

Throws `std::runtime_error("max agent depth exceeded")` if `current_depth >= max_depth`.

**Thread safety:** `AgentManager` methods are individually thread-safe; concurrent `TaskAction` calls on the pool are safe.

---

### TodoWriteAction

**Factory name:** `"TodoWriteAction"`  
**File:** `actions/todo_write_action.hpp` / `todo_write_action.cpp`

Manages the per-agent todo list stored in `AgentContext::todo_list`. Supports four operations: `add`, `remove`, `clear`, `list`.

**Inputs:**
```json
{
  "operation": "add",
  "item":      "Review PR #42"
}
```

| Operation | Description |
|---|---|
| `"add"` | Append `item` to the list |
| `"remove"` | Remove first occurrence of `item` |
| `"clear"` | Empty the entire list |
| `"list"` | Return the list (no mutation) |

**Output:**
```json
{"todo_list": ["Review PR #42"], "size": 1}
```

**Thread safety:** `todo_list` is only accessed from the agent loop thread — no concurrent access by design.

---

## Messaging Actions (Pattern B)

### SendMessageAction

**Factory name:** `"SendMessageAction"`  
**File:** `actions/messaging_actions.hpp` / `messaging_actions.cpp`

Sends a JSON message to another agent's `MessageInbox` via `AgentManager::sendMessage`.

**Inputs:**
```json
{
  "to_id":   "agent_2",
  "payload": {"type": "result", "data": "$my_result"}
}
```

**Output:**
```json
{"sent": true, "to": "agent_2"}
```

---

### ReceiveMessagesAction

**Factory name:** `"ReceiveMessagesAction"`  
**File:** `actions/messaging_actions.hpp` / `messaging_actions.cpp`

Drains this agent's inbox and returns all pending messages.

**Inputs:** `{}`

**Output:**
```json
{
  "messages": [
    {"from_id": "agent_1", "payload": {...}, "timestamp": "2026-06-01T12:00:00Z"}
  ],
  "count": 1
}
```

---

## Blackboard Actions (Pattern C)

### BlackboardWriteAction

**Factory name:** `"BlackboardWriteAction"`  
**File:** `actions/blackboard_actions.hpp` / `blackboard_actions.cpp`

Writes a key-value pair to the shared `Blackboard`.

**Inputs:**
```json
{"key": "research.summary", "value": "$transform_result"}
```

**Output:**
```json
{"written": true, "key": "research.summary"}
```

---

### BlackboardReadAction

**Factory name:** `"BlackboardReadAction"`  
**File:** `actions/blackboard_actions.hpp` / `blackboard_actions.cpp`

Reads a value from the shared `Blackboard` by key.

**Inputs:**
```json
{"key": "research.summary"}
```

**Output:**
```json
{"key": "research.summary", "value": "...", "found": true}
```

Returns `{"found": false}` if the key does not exist.

---

### BlackboardListAction

**Factory name:** `"BlackboardListAction"`  
**File:** `actions/blackboard_actions.hpp` / `blackboard_actions.cpp`

Lists all keys in the `Blackboard`, optionally filtered by prefix.

**Inputs:**
```json
{"prefix": "research."}
```

**Output:**
```json
{"keys": ["research.summary", "research.sources"], "count": 2}
```

---

## Memory Actions

### MemoryWriteAction

**Factory name:** `"MemoryWriteAction"`  
**File:** `actions/memory_actions.hpp` / `memory_actions.cpp`

Persists a string entry to the agent's `MemoryBackend` (vector DB, SQLite, etc.).

**Inputs:**
```json
{
  "id":       "mem_001",
  "content":  "The project uses C++17 with CMake.",
  "metadata": {"source": "README"}
}
```

**Output:**
```json
{"written": true, "id": "mem_001"}
```

---

### MemoryReadAction

**Factory name:** `"MemoryReadAction"`

Performs semantic or keyword search over the `MemoryBackend`.

**Inputs:**
```json
{"query": "C++ build system", "top_k": 5}
```

**Output:**
```json
{
  "results": [
    {"id": "mem_001", "content": "...", "score": 0.92, "metadata": {}}
  ]
}
```

---

### MemoryListAction

**Factory name:** `"MemoryListAction"`

Lists all memory entries, optionally filtered by a string prefix/substring.

**Inputs:**
```json
{"filter": ""}
```

**Output:**
```json
{"entries": [...], "count": 5}
```

---

## MCP Tool Actions

### MCPToolAction

**Factory name:** `"MCPToolAction"`  
**File:** `actions/mcp_tool_action.hpp` / `mcp_tool_action.cpp`

Dispatches a call to a registered MCP (Model Context Protocol) server tool. Looks up the server connection from `AgentManager::m_mcp_servers` and sends the tool call with a unique correlation ID.

As shipped this is a stub; a full implementation would use the MCP client library registered via `AgentManager::connectMCP`.

**Construction:**
```cpp
MCPToolAction(std::string id,
              std::string tool_name,
              std::string server_name,
              nlohmann::json inputs = {});
```

**Inputs:** Tool-specific; passed through to the server.

**Output:**
```json
{"tool": "my_tool", "server": "myserver", "result": {...}}
```

---

## Registration Summary

| Free function | Registers |
|---|---|
| `registerBashAction` | `BashAction` |
| `registerReadAction` | `ReadAction` |
| `registerWriteAction` | `WriteAction` |
| `registerEditAction` | `EditAction` |
| `registerGlobAction` | `GlobAction` |
| `registerGrepAction` | `GrepAction` |
| `registerWebFetchAction` | `WebFetchAction` |
| `registerWebSearchAction` | `WebSearchAction` |
| `registerTaskAction` | `TaskAction` |
| `registerTodoWriteAction` | `TodoWriteAction` |
| `registerMessagingActions` | `SendMessageAction`, `ReceiveMessagesAction` |
| `registerBlackboardActions` | `BlackboardWriteAction`, `BlackboardReadAction`, `BlackboardListAction` |
| `registerMemoryActions` | `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction` |
| `registerMCPToolAction` | `MCPToolAction` |

---

## Related Components

- [`WorkItem`](work_item.md) — base class for all actions
- [`AgentContext`](agent.md) — provides LLM, blackboard, inbox, memory, manager access
- [`BatchExecutor`](batch_executor.md) — executes actions in parallel when deps allow
- [`Blackboard`](blackboard.md) — target of Pattern C actions
- [`MessageInbox`](message_inbox.md) — target of Pattern B actions
- [`Stages`](stages.md) — LLM-powered counterparts to deterministic actions
