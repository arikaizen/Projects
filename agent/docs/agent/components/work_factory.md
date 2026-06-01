# WorkFactory

`include/agent/work_factory.hpp` · `src/agent/work_factory.cpp`

## Overview

`WorkFactory` is a self-registering factory and catalog for `WorkItem` types. The LLM names items by string; the factory constructs them. Adding a new item type requires only calling `registerItem` once — no modifications to `WorkFactory` itself.

## `WorkItemSpec`

```cpp
struct WorkItemSpec {
    std::string    name;
    std::string    description;
    WorkItem::Kind kind;
    nlohmann::json input_schema;   // JSON Schema for the inputs object
};
```

`input_schema` is injected into the `{{CATALOG}}` prompt placeholder so the LLM knows what inputs each item type accepts.

## Registration

```cpp
using CreateFn = std::function<std::unique_ptr<WorkItem>(std::string id, nlohmann::json inputs)>;

void registerItem(WorkItemSpec spec, CreateFn fn);
```

Typically called once from a free registration function:

```cpp
void registerBashAction(WorkFactory& factory) {
    factory.registerItem(
        {"BashAction", "Run a shell command", WorkItem::Kind::Action,
         {{"type","object"},{"required",{"command"}},
          {"properties",{{"command",{{"type","string"}}}}}}},
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<BashAction>(std::move(id), "BashAction", std::move(inputs));
        }
    );
}
```

`AgentManager` calls all `register*` functions during construction via `registerBuiltinItems()`.

## Creation

```cpp
std::unique_ptr<WorkItem> create(const std::string& name,
                                 const std::string& id,
                                 const nlohmann::json& inputs) const;
```

Looks up `name` in the registry and calls the stored `CreateFn`. Throws `std::runtime_error` if `name` is not registered.

## Catalog

```cpp
nlohmann::json            toCatalogJson()  const;
std::vector<WorkItemSpec> listSpecs()      const;
const nlohmann::json*     inputSchema(const std::string& name) const;
bool                      isRegistered(const std::string& name) const;
```

`toCatalogJson()` returns a JSON array of all registered specs. `ReasonStage` and `InjectionStage` use this to fill the `{{CATALOG}}` placeholder in the system prompt, giving the LLM a complete menu of available items with their input schemas.

`inputSchema` returns a pointer to the stored schema (or `nullptr`) for per-item validation.

## Thread-Safety

`WorkFactory` uses a `std::shared_mutex`:
- `registerItem` acquires a `unique_lock` (exclusive write).
- `create`, `isRegistered`, `listSpecs`, `toCatalogJson`, `inputSchema` each acquire a `shared_lock` (concurrent reads).

Registration happens exclusively during `AgentManager` construction, before any agents run. Subsequent access is read-only and fully concurrent.

## Related Components

- [`WorkItem`](work_item.md) — the base class of all registered types
- [`ReasonStage`](reason_stage.md) — uses `toCatalogJson()` to populate `{{CATALOG}}`
- [`InjectionStage`](injection_stage.md) — same
- [`AgentManager`](agent_manager.md) — owns the factory; calls all `register*` functions at startup
- [`AgentContext`](agent_context.md) — holds a `shared_ptr<WorkFactory>`
