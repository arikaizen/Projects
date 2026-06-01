# Action (base class)

`include/agent/action.hpp`

## Overview

`Action` is the marker base class for all deterministic operations. It inherits from `WorkItem` and overrides `kind()` to return `Kind::Action`.

```cpp
class Action : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Action; }
};
```

No additional logic is added here. All behavior is in the concrete subclasses.

## Role in the Engine

Actions are the items that `BatchExecutor` runs in parallel. Because they are deterministic (no LLM calls), they can safely execute concurrently as long as their `$ref` dependencies allow it.

Actions that write to the same file must be ordered via `$ref` dependencies. For example, if `WriteAction` writes a file and `EditAction` modifies it, `EditAction`'s `path` input should reference the write result via `$step_id.path` to force sequential ordering.

## Common Pattern

Every concrete action:
1. Calls `ctx.resolveReferences(inputs)` to expand any `$ref` values.
2. Performs its operation.
3. Returns a `WorkResult` with `item_kind = "Action"`, `success`, `output`, and `error` fields populated.
4. Records `duration` via a start/end `std::chrono::steady_clock` pair.

## Built-in Actions (13 types)

| Action | Factory name | Category |
|---|---|---|
| `BashAction` | `"BashAction"` | Shell |
| `ReadAction` | `"ReadAction"` | Filesystem |
| `WriteAction` | `"WriteAction"` | Filesystem |
| `EditAction` | `"EditAction"` | Filesystem |
| `GlobAction` | `"GlobAction"` | Filesystem |
| `GrepAction` | `"GrepAction"` | Filesystem |
| `WebFetchAction` | `"WebFetchAction"` | Network |
| `WebSearchAction` | `"WebSearchAction"` | Network |
| `TaskAction` | `"TaskAction"` | Multi-agent (Pattern A) |
| `TodoWriteAction` | `"TodoWriteAction"` | Agent state |
| `SendMessageAction` | `"SendMessageAction"` | Messaging (Pattern B) |
| `ReceiveMessagesAction` | `"ReceiveMessagesAction"` | Messaging (Pattern B) |
| `BlackboardWriteAction` | `"BlackboardWriteAction"` | Blackboard (Pattern C) |
| `BlackboardReadAction` | `"BlackboardReadAction"` | Blackboard (Pattern C) |
| `BlackboardListAction` | `"BlackboardListAction"` | Blackboard (Pattern C) |
| `MemoryWriteAction` | `"MemoryWriteAction"` | Long-term memory |
| `MemoryReadAction` | `"MemoryReadAction"` | Long-term memory |
| `MemoryListAction` | `"MemoryListAction"` | Long-term memory |
| `MCPToolAction` | `"MCPToolAction"` | MCP |

See [actions.md](actions.md) for the full overview.

## Related Components

- [`WorkItem`](work_item.md) — base class
- [`Stage`](stage.md) — sibling base for LLM-powered items
- [`actions.md`](actions.md) — overview of all built-in actions
- [`BatchExecutor`](batch_executor.md) — executes actions in parallel
