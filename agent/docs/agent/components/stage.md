# Stage (base class)

`include/agent/stage.hpp`

## Overview

`Stage` is the marker base class for all LLM-powered reasoning steps. It inherits from `WorkItem` and overrides `kind()` to return `Kind::Stage`.

```cpp
class Stage : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Stage; }
};
```

No additional logic is added here. All behavior is in the concrete subclasses.

## Role in the Engine

`BatchExecutor` does not treat stages differently from actions during parallel scheduling. The sequential ordering of stages relative to actions within a batch is a natural consequence of how the `Agent` loop works:

1. The agent pops a stage from the queue.
2. The stage executes and pushes action items onto the queue.
3. The loop drains those actions via `try_pop()`.
4. The entire drain (stage + actions) forms a single batch submitted to `BatchExecutor`.

Because the stage is always the first item in the batch and actions depend on its output (implicitly, through LLM-generated plan ordering), they execute after it.

## Built-in Stages

| Stage | Factory name | Purpose |
|---|---|---|
| `ReasonStage` | `"ReasonStage"` | Primary planning — surveys state and generates a full plan |
| `InjectionStage` | `"InjectionStage"` | Meta-planning — inspects the last result and injects follow-up items |
| `TransformStage` | `"TransformStage"` | LLM text transformation with an instruction and input text |
| `ValidateStage` | `"ValidateStage"` | LLM-powered validation with optional corrective injection |

See [stages.md](stages.md) for the full overview.

## Related Components

- [`WorkItem`](work_item.md) — base class
- [`Action`](action.md) — sibling base for deterministic items
- [`stages.md`](stages.md) — overview of all built-in stages
