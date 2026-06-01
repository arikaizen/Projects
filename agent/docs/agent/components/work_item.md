# WorkItem & WorkResult

`include/agent/work_item.hpp` · `src/agent/work_item.cpp`

## WorkItem

### Overview

`WorkItem` is the abstract base class for everything the agent executes. Both `Stage` (LLM-powered reasoning) and `Action` (deterministic operations) derive from it. All items live in the same queue and execute through the same `BatchExecutor`.

### Class Definition

```cpp
class WorkItem {
public:
    enum class Kind { Stage, Action };

    std::string    id;
    std::string    name;
    nlohmann::json inputs;

    WorkItem(std::string id_, std::string name_, nlohmann::json inputs_ = {});

    virtual Kind        kind()        const = 0;
    virtual WorkResult  execute(AgentContext& ctx) = 0;
    virtual std::string description() const { return name; }

    std::set<std::string> dependencies() const;
    nlohmann::json        toSummaryJson() const;
};
```

### Fields

| Field | Meaning |
|---|---|
| `id` | Unique string within an agent run. Used by `$ref` resolution. Must be unique across the agent's history. |
| `name` | Registered factory key (e.g. `"BashAction"`, `"ReasonStage"`). Used by `WorkFactory::create`. |
| `inputs` | JSON object carrying parameters. String values matching `^\$([a-zA-Z0-9_]+)(\.[a-zA-Z0-9_]+)?$` are `$ref` references. |

### `dependencies()`

```cpp
std::set<std::string> dependencies() const;
```

Scans `inputs` recursively for `$ref` patterns and returns the set of referenced item ids. `BatchExecutor` calls this during DAG construction to determine which items must complete before this one can start.

### `$ref` Syntax

| Pattern | Resolves to |
|---|---|
| `"$step1"` | `resultById("step1").output` |
| `"$step1.field"` | `resultById("step1").output["field"]` |

References are resolved by `AgentContext::resolveReferences` before execution.

---

## WorkResult

Recorded for every executed `WorkItem`, whether it succeeded, failed, or was skipped.

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
    std::string    skipped_reason;
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

`toJson()` serialises all fields.

---

## Related Components

- [`Stage`](stage.md) — `Kind::Stage` subclass marker
- [`Action`](action.md) — `Kind::Action` subclass marker
- [`WorkFactory`](work_factory.md) — creates `WorkItem` subclasses by name
- [`BatchExecutor`](batch_executor.md) — uses `dependencies()` to build the DAG
- [`AgentContext`](agent_context.md) — stores `WorkResult` in history; resolves `$ref` in inputs
