# Stage (base class)

`include/agent/stage.hpp`

---

## Overview

`Stage` is the abstract marker base for all **LLM-powered reasoning** work items. It derives from [`WorkItem`](work_item.md) and fixes its kind to `Kind::Stage`.

```cpp
class Stage : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Stage; }
};
```

That single override is the entire class — `Stage` adds no data. Its purpose is to **classify** an item so the engine can treat reasoning steps differently from deterministic [`Action`](action.md)s.

---

## Why the distinction matters

- **Sequencing:** stages are processed so each sees the full prior history before the next reasoning step runs; the engine does not parallelise reasoning the way it parallelises independent actions.
- **Catalog & events:** `WorkResult::item_kind` is reported as `"Stage"`, and the factory catalog labels the item `Kind::Stage`.

---

## Concrete Stages

| Class | Doc |
|---|---|
| `ReasonStage` | [reason_stage.md](reason_stage.md) |
| `InjectionStage` | [injection_stage.md](injection_stage.md) |
| `TransformStage` | [transform_stage.md](transform_stage.md) |
| `ValidateStage` | [validate_stage.md](validate_stage.md) |

---

## Writing a Stage

```cpp
class MyStage : public Stage {
public:
    MyStage(std::string id, nlohmann::json inputs = {})
        : Stage(std::move(id), "MyStage", std::move(inputs)) {}
    WorkResult execute(AgentContext& ctx) override { /* render prompt, call ctx.llm(), ... */ }
};
void registerMyStage(WorkFactory& f);   // factory.registerItem(spec, ctor)
```

---

## Related

- [WorkItem](work_item.md) — base of `Stage`
- [Action](action.md) — the deterministic sibling base
- [Stages overview](stages.md) — the four built-ins
- [WorkFactory](work_factory.md) — registration
