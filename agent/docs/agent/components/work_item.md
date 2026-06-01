# WorkItem & WorkResult

`include/agent/work_item.hpp` · `src/agent/work_item.cpp`

> Related components have their own pages:
> [WorkFactory](work_factory.md) · [Stage (base)](stage.md) · [Action (base)](action.md).
> This page covers `WorkItem` and `WorkResult`; the factory/base sections below are summaries that link out.

---

## WorkItem

### Overview

`WorkItem` is the abstract base class for everything the agent executes. Both `Stage` (LLM-powered reasoning) and `Action` (deterministic operations) derive from it. All items live in the same queue and are executed by the same `BatchExecutor`.

### Class Definition

```cpp
class WorkItem {
public:
    enum class Kind { Stage, Action };

    std::string    id;       // unique within an agent's history
    std::string    name;     // registered name in WorkFactory (e.g. "BashAction")
    nlohmann::json inputs;   // parameters; may contain $ref strings

    virtual Kind        kind()        const = 0;
    virtual WorkResult  execute(AgentContext& ctx) = 0;
    virtual std::string description() const;

    std::set<std::string> dependencies() const;
    nlohmann::json        toSummaryJson() const;
};
```

### `id`

Unique string identifier for this item within an agent run. Used by `$ref` resolution (`"$step1"` looks up the item with `id == "step1"`). Must be unique across the agent's history.

### `name`

The registered factory key (e.g. `"BashAction"`, `"ReasonStage"`). Used by `WorkFactory::create` to construct the correct subclass.

### `inputs`

JSON object carrying parameters. String values matching `^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$` are treated as references:

| Pattern | Resolves to |
|---|---|
| `"$step1"` | `resultById("step1").output` |
| `"$step1.field"` | `resultById("step1").output["field"]` |

References are resolved by `AgentContext::resolveReferences` before execution.

### `dependencies()`

Scans `inputs` recursively for `$ref` patterns and returns the set of referenced item ids. `BatchExecutor` calls this during DAG construction to determine which items must complete before this one can start.

---

## WorkResult

Recorded for every executed `WorkItem`, whether it succeeded or was skipped.

```cpp
struct WorkResult {
    std::string    item_id;
    std::string    item_name;
    std::string    item_kind;           // "Stage" or "Action"
    bool           success{false};
    nlohmann::json output;
    std::string    error;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds             duration{0};
    int            iteration{0};
    bool           ran_in_parallel{false};
    std::string    skipped_reason;      // non-empty if skipped
};
```

| Field | Meaning |
|---|---|
| `item_id` | The `WorkItem::id` |
| `item_name` | The `WorkItem::name` |
| `item_kind` | `"Stage"` or `"Action"` |
| `success` | `true` if `execute` completed without exception |
| `output` | The JSON value returned by `execute` |
| `error` | Exception message if `success == false` |
| `timestamp` | Wall-clock time at execution start |
| `duration` | Elapsed time for the `execute` call |
| `iteration` | `AgentContext::iteration_count` at recording time |
| `ran_in_parallel` | `true` when sibling items ran concurrently in the same batch |
| `skipped_reason` | Non-empty when the item was skipped due to a failed dependency |

`toJson()` serialises all fields to a JSON object.

---

## Stage

```cpp
class Stage : public WorkItem {
    Kind kind() const override { return Kind::Stage; }
};
```

Marker subclass for LLM-powered reasoning steps. Stages execute **sequentially** relative to each other within a batch: the loop processes a full stage-generated plan before allowing parallel action execution.

Built-in stages: `ReasonStage`, `InjectionStage`, `TransformStage`, `ValidateStage`. See [`stages.md`](stages.md).

---

## Action

```cpp
class Action : public WorkItem {
    Kind kind() const override { return Kind::Action; }
};
```

Marker subclass for deterministic operations (filesystem, shell, web, messaging). Actions are the items that `BatchExecutor` executes in parallel.

Built-in actions: 13 types. See [`actions.md`](actions.md).

---

## WorkFactory

### Overview

`WorkFactory` is a self-registering factory and catalog for `WorkItem` types. The LLM names items by string; the factory builds them. Adding a new item type requires only calling `registerItem` — no modifications to `WorkFactory` itself.

### `WorkItemSpec`

```cpp
struct WorkItemSpec {
    std::string    name;
    std::string    description;
    WorkItem::Kind kind;
    nlohmann::json input_schema;   // JSON Schema for the inputs object
};
```

`input_schema` is injected into the `{{CATALOG}}` prompt placeholder so the LLM knows what inputs each item accepts.

### Registration

```cpp
using CreateFn = std::function<std::unique_ptr<WorkItem>(std::string id, nlohmann::json inputs)>;

void registerItem(WorkItemSpec spec, CreateFn fn);
```

Typically called from a free function like `registerBashAction(factory)`:

```cpp
void registerBashAction(WorkFactory& factory) {
    factory.registerItem(
        {"BashAction", "Run a shell command", WorkItem::Kind::Action,
         {{"type","object"},{"properties",{{"command",{{"type","string"}}}}}}},
        [](std::string id, nlohmann::json inputs) {
            return std::make_unique<BashAction>(std::move(id), "BashAction", std::move(inputs));
        }
    );
}
```

`AgentManager` calls all `register*` functions during construction.

### Creation

```cpp
std::unique_ptr<WorkItem> create(const std::string& name,
                                 const std::string& id,
                                 const nlohmann::json& inputs) const;
```

Looks up `name` in the registry and calls the stored `CreateFn`. Throws `std::runtime_error` if `name` is not registered.

### Catalog

```cpp
nlohmann::json toCatalogJson() const;
std::vector<WorkItemSpec> listSpecs() const;
const nlohmann::json* inputSchema(const std::string& name) const;
bool isRegistered(const std::string& name) const;
```

`toCatalogJson()` returns a JSON array of all specs. Used by `ReasonStage` to fill the `{{CATALOG}}` placeholder in the system prompt, giving the LLM a complete menu of available items with their input schemas.

### Thread-Safety

`WorkFactory` uses a `std::shared_mutex`:
- `registerItem` acquires a `unique_lock` (exclusive write).
- `create`, `isRegistered`, `listSpecs`, `toCatalogJson`, `inputSchema` acquire a `shared_lock` (concurrent reads).

---

## Related Components

- [`BatchExecutor`](batch_executor.md) — executes items, builds DAG from `dependencies()`
- [`AgentContext`](agent.md) — resolves `$ref` in inputs, stores `WorkResult` in history
- [`stages.md`](stages.md) — built-in `Stage` subclasses
- [`actions.md`](actions.md) — built-in `Action` subclasses
- [`PromptLoader`](prompt_loader.md) — `{{CATALOG}}` placeholder filled from `toCatalogJson()`
