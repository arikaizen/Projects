# WorkFactory

`include/agent/work_factory.hpp` · `src/agent/work_factory.cpp`

---

## Overview

`WorkFactory` is a self-registering factory and catalog for `WorkItem` types. The LLM refers to items by string name; the factory constructs the correct C++ subclass. Adding a new item type requires only calling `registerItem` — no edits to `WorkFactory` itself.

It is shared across all agents via `AgentManager` and guarded by a `std::shared_mutex` (concurrent reads, exclusive writes).

---

## WorkItemSpec

```cpp
struct WorkItemSpec {
    std::string    name;          // factory key, e.g. "BashAction"
    std::string    description;   // human/LLM-facing summary
    WorkItem::Kind kind;          // Stage or Action
    nlohmann::json input_schema;  // JSON Schema for the inputs object
};
```

`input_schema` is surfaced to the LLM via the `{{CATALOG}}` prompt placeholder so it knows each item's accepted inputs.

---

## Registration

```cpp
using CreateFn = std::function<std::unique_ptr<WorkItem>(std::string id, nlohmann::json inputs)>;
void registerItem(WorkItemSpec spec, CreateFn fn);
```

Typically invoked from a free function per item type:

```cpp
void registerBashAction(WorkFactory& factory) {
    WorkItemSpec spec{ "BashAction", "Run a shell command", WorkItem::Kind::Action,
        {{"type","object"}, {"required",{"command"}},
         {"properties", {{"command", {{"type","string"}}}}}} };
    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<BashAction>(std::move(id), "BashAction", std::move(inputs));
    });
}
```

`AgentManager::registerBuiltinItems()` calls every `register*` function during construction.

---

## Creation & Query

```cpp
std::unique_ptr<WorkItem> create(const std::string& name,
                                 const std::string& id,
                                 const nlohmann::json& inputs) const;  // throws if unknown

bool                      isRegistered(const std::string& name) const;
std::vector<WorkItemSpec> listSpecs() const;
nlohmann::json            toCatalogJson() const;          // for {{CATALOG}}
const nlohmann::json*     inputSchema(const std::string& name) const;
```

`toCatalogJson()` returns the full catalog; the planning stages embed it so the LLM has a complete menu of items and schemas. `create` throws `std::runtime_error` for an unregistered name — `ReasonStage`/`InjectionStage` guard against this during plan validation via `isRegistered`.

---

## Thread-Safety

| Operation | Lock |
|---|---|
| `registerItem` | `unique_lock` (exclusive) |
| `create`, `isRegistered`, `listSpecs`, `toCatalogJson`, `inputSchema` | `shared_lock` (concurrent) |

---

## Related

- [WorkItem](work_item.md) — the items produced
- [Stage](stage.md) / [Action](action.md) — the two base kinds
- [Stages](stages.md) · [Actions](actions.md) — the registered built-ins
- [AgentContext](agent_context.md) — exposes `factory()` to stages
- [PromptLoader](prompt_loader.md) — `{{CATALOG}}` source
