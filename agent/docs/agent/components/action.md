# Action (base class)

`include/agent/action.hpp`

---

## Overview

`Action` is the abstract marker base for all **deterministic** work items — file I/O, shell, web, messaging, blackboard, memory, MCP. It derives from [`WorkItem`](work_item.md) and fixes its kind to `Kind::Action`.

```cpp
class Action : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Action; }
};
```

Like [`Stage`](stage.md), it adds no data — it exists to **classify** the item.

---

## Why the distinction matters

Actions are the items the engine runs **in parallel**. Within a batch, all actions whose `$ref` dependencies are satisfied are dispatched concurrently on the [`ThreadPool`](thread_pool.md) by the [`BatchExecutor`](batch_executor.md). `WorkResult::item_kind` is reported as `"Action"`.

---

## Concrete Actions (13 built-in)

| Category | Classes | Doc |
|---|---|---|
| Shell | `BashAction` | [bash_action.md](bash_action.md) |
| Files | `ReadAction`, `WriteAction`, `EditAction` | [read](read_action.md) · [write](write_action.md) · [edit](edit_action.md) |
| Search | `GlobAction`, `GrepAction` | [glob](glob_action.md) · [grep](grep_action.md) |
| Web | `WebFetchAction`, `WebSearchAction` | [fetch](web_fetch_action.md) · [search](web_search_action.md) |
| Agents | `TaskAction` | [task_action.md](task_action.md) |
| Todo | `TodoWriteAction` | [todo_write_action.md](todo_write_action.md) |
| Messaging (B) | `SendMessageAction`, `ReceiveMessagesAction` | [messaging_actions.md](messaging_actions.md) |
| Blackboard (C) | `BlackboardWrite/Read/List` | [blackboard_actions.md](blackboard_actions.md) |
| Memory | `MemoryWrite/Read/List` | [memory_actions.md](memory_actions.md) |
| MCP | `MCPToolAction` | [mcp_tool_action.md](mcp_tool_action.md) |

---

## Writing an Action

```cpp
class MyAction : public Action {
public:
    MyAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}
    WorkResult execute(AgentContext& ctx) override {
        auto resolved = ctx.resolveReferences(inputs);   // resolve $ref first
        /* do the deterministic work ... */
    }
};
void registerMyAction(WorkFactory& f);
```

Actions that share a path/file are not safe to run concurrently — express ordering with `$ref` dependencies so the [`BatchExecutor`](batch_executor.md) serialises them.

---

## Related

- [WorkItem](work_item.md) — base of `Action`
- [Stage](stage.md) — the LLM-powered sibling base
- [Actions overview](actions.md) — the built-ins
- [BatchExecutor](batch_executor.md) — parallel execution · [WorkFactory](work_factory.md) — registration
