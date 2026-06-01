# Actions (overview)

`src/agent/actions/` — deterministic work items deriving from [`Action`](action.md) → [`WorkItem`](work_item.md).

Actions perform side-effectful, non-LLM operations and are the items the [`BatchExecutor`](batch_executor.md) runs in parallel (subject to `$ref` dependencies). Each registers with the [`WorkFactory`](work_factory.md) during `AgentManager` construction.

> Each action (or action group) has its own detailed page — this is the index.

| Action(s) | Factory name(s) | Purpose | Doc |
|---|---|---|---|
| BashAction | `BashAction` | Run a shell command | [bash_action.md](bash_action.md) |
| ReadAction | `ReadAction` | Read a file (optional line range) | [read_action.md](read_action.md) |
| WriteAction | `WriteAction` | Write a file | [write_action.md](write_action.md) |
| EditAction | `EditAction` | Exact string replace in a file | [edit_action.md](edit_action.md) |
| GlobAction | `GlobAction` | Glob the filesystem | [glob_action.md](glob_action.md) |
| GrepAction | `GrepAction` | Search file contents (rg or fallback) | [grep_action.md](grep_action.md) |
| WebFetchAction | `WebFetchAction` | HTTP/HTTPS fetch | [web_fetch_action.md](web_fetch_action.md) |
| WebSearchAction | `WebSearchAction` | Web search (stub) | [web_search_action.md](web_search_action.md) |
| TaskAction | `TaskAction` | Spawn a sub-agent (Pattern A) | [task_action.md](task_action.md) |
| TodoWriteAction | `TodoWriteAction` | Manage the per-agent todo list | [todo_write_action.md](todo_write_action.md) |
| Messaging | `SendMessageAction`, `ReceiveMessagesAction` | Pattern B messaging | [messaging_actions.md](messaging_actions.md) |
| Blackboard | `BlackboardWrite/Read/List` | Pattern C shared state | [blackboard_actions.md](blackboard_actions.md) |
| Memory | `MemoryWrite/Read/List` | Long-term memory | [memory_actions.md](memory_actions.md) |
| MCPToolAction | `MCPToolAction` | Call an MCP server tool (stub) | [mcp_tool_action.md](mcp_tool_action.md) |

## Common shape

Every action sets `WorkResult.item_kind = "Action"`, resolves `$ref` inputs via `ctx.resolveReferences(inputs)` before doing work, and reports `{success, output, error, duration}`. Actions that touch the same path (write/edit) must be ordered via `$ref` dependencies because the batch runs independent items concurrently.

## Stub status

| Action | Status |
|---|---|
| `WebFetchAction` | Real (httplib or curl) |
| `WebSearchAction` | ⚠️ Stub — needs a search API key |
| `MCPToolAction` | ⚠️ Stub — needs MCP client wiring |
| `Memory*Action` | Inert with `NoOpMemoryBackend`; real with [AIModelMemoryBackend](ai_model_memory_backend.md) |
| all others | Real |

## Related

- [Action (base class)](action.md) · [Stages overview](stages.md)
- [BatchExecutor](batch_executor.md) · [WorkFactory](work_factory.md) · [AgentContext](agent_context.md)
