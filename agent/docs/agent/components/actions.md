# Actions (overview)

`src/agent/actions/` — deterministic work items deriving from [`Action`](action.md) → [`WorkItem`](work_item.md).

Actions perform side-effectful, non-LLM operations. They are the items `BatchExecutor` runs in parallel (subject to `$ref` dependencies). Each registers with `WorkFactory` during `AgentManager` construction.

## Index

| Action(s) | Factory name(s) | Purpose | Doc |
|---|---|---|---|
| BashAction | `BashAction` | Run a shell command | [bash_action.md](bash_action.md) |
| ReadAction | `ReadAction` | Read a file (optional line range) | [read_action.md](read_action.md) |
| WriteAction | `WriteAction` | Write a file (truncating) | [write_action.md](write_action.md) |
| EditAction | `EditAction` | Replace first occurrence in a file | [edit_action.md](edit_action.md) |
| GlobAction | `GlobAction` | Glob the filesystem (`*` and `?`) | [glob_action.md](glob_action.md) |
| GrepAction | `GrepAction` | Search file contents (rg or fallback) | [grep_action.md](grep_action.md) |
| WebFetchAction | `WebFetchAction` | HTTP/HTTPS fetch | [web_fetch_action.md](web_fetch_action.md) |
| WebSearchAction | `WebSearchAction` | Web search (stub) | [web_search_action.md](web_search_action.md) |
| TaskAction | `TaskAction` | Spawn a sub-agent (Pattern A) | [task_action.md](task_action.md) |
| TodoWriteAction | `TodoWriteAction` | Manage the per-agent todo list | [todo_write_action.md](todo_write_action.md) |
| Messaging | `SendMessageAction`, `ReceiveMessagesAction` | Pattern B messaging | [messaging_actions.md](messaging_actions.md) |
| Blackboard | `BlackboardWriteAction`, `BlackboardReadAction`, `BlackboardListAction` | Pattern C shared state | [blackboard_actions.md](blackboard_actions.md) |
| Memory | `MemoryWriteAction`, `MemoryReadAction`, `MemoryListAction` | Long-term memory | [memory_actions.md](memory_actions.md) |
| MCPToolAction | `MCPToolAction` | Call an MCP server tool (stub) | [mcp_tool_action.md](mcp_tool_action.md) |

## Common Shape

Every action:
1. Sets `item_kind = "Action"` on the result.
2. Calls `ctx.resolveReferences(inputs)` to expand `$ref` values before doing work.
3. Populates `{success, output, error, duration}` on the `WorkResult`.
4. Records `duration` via `std::chrono::steady_clock`.

## Stub Status

| Action | Status |
|---|---|
| `WebFetchAction` | Real — uses cpp-httplib if available, otherwise falls back to `curl` |
| `WebSearchAction` | Stub — requires `SEARCH_API_KEY` and search provider config |
| `MCPToolAction` | Stub — requires `AgentManager::connectMCP()` to be wired up |
| `Memory*Action` | Inert with `NoOpMemoryBackend`; real with [`AIModelMemoryBackend`](ai_model_memory_backend.md) |
| All others | Real |

## Related

- [Action (base class)](action.md) · [Stages overview](stages.md)
- [BatchExecutor](batch_executor.md) · [WorkFactory](work_factory.md)
