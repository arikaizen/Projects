# Built-in Stages

`src/agent/stages/`

All four built-in stages are subclasses of `Stage` → `WorkItem`. Each renders a prompt template from `PromptLoader`, calls the LLM, interprets the response, and optionally pushes new items onto the agent queue.

Stages are registered with `WorkFactory` via free functions (`registerReasonStage`, etc.) called during `AgentManager` construction.

---

## ReasonStage

**File:** `stages/reason_stage.hpp` / `reason_stage.cpp`  
**Factory name:** `"ReasonStage"`  
**Prompt template:** `prompts/reason_stage.md`

### Purpose

Primary reasoning step (Reading 1). Given the agent's full state — task, history, available tool catalog, and todo list — it asks the LLM to produce an **execution plan**: a JSON array of work items to run next.

### Inputs

```json
{ "task": "optional override task string" }
```

If `task` is absent, `AgentContext::config().task` is used.

### Execution

1. Builds prompt variables:
   - `{{TASK}}` — the agent's task string
   - `{{HISTORY}}` — `ctx.historySummaryJson(20)` serialised to string
   - `{{CATALOG}}` — `ctx.factory().toCatalogJson()` serialised to string
   - `{{TODO}}` — `ctx.todo_list` joined as newline-separated string
2. Renders `reason_stage.md` via `PromptLoader::render`.
3. Calls `ctx.llm().complete(request)` in JSON mode.
4. Parses the LLM response as `{"plan": [...]}`.
5. Validates each plan item: checks that the name is registered in `WorkFactory` and that the id is not already in history.
6. Constructs each `WorkItem` via `WorkFactory::create`.
7. Pushes all plan items onto the queue (back, preserving plan order).

### Output

```json
{"plan_size": 3, "items": ["fetch", "parse", "report"]}
```

### Error Handling

If the LLM returns invalid JSON or the plan contains unknown item names, `ReasonStage` records a failed `WorkResult` and does **not** push anything. The agent loop will either call another stage or terminate via `QueueEmpty`.

### `validateAndPushPlan`

Internal helper. Validates each plan entry, constructs items, and pushes them. Returns `false` and sets `error_out` on the first invalid entry.

---

## InjectionStage

**File:** `stages/injection_stage.hpp` / `injection_stage.cpp`  
**Factory name:** `"InjectionStage"`  
**Prompt template:** `prompts/injection_stage.md`

### Purpose

Meta-stage (Reading 2). Operates on the **previous item's output** and decides what to inject next. Distinct from `ReasonStage` — its prompt is narrower: "given this output, what should happen next?" rather than "given the whole agent state, what is the full plan?"

Typical usage: pushed onto the queue immediately after a long-running action to decide whether to retry, transform the output, or signal completion.

### Inputs

```json
{
  "target_id":   "step3",     // id of the item whose output to inspect
  "context":     "optional additional context string"
}
```

### Execution

1. Looks up `resultById(target_id)` in history.
2. Builds prompt with `{{PREVIOUS_OUTPUT}}`, `{{CATALOG}}`, `{{HISTORY}}`.
3. Calls LLM; parses `{"inject": [...]}` or `{"done": true}`.
4. If `done`, sets `ctx.should_stop = true`.
5. Otherwise validates and pushes the `inject` array.

### Output

```json
{"injected": 2}
```
or
```json
{"done": true}
```

---

## TransformStage

**File:** `stages/transform_stage.hpp` / `transform_stage.cpp`  
**Factory name:** `"TransformStage"`  
**Prompt template:** `prompts/transform_stage.md`

### Purpose

General-purpose LLM text transformation. Takes an instruction and an input text (or a `$ref` to a previous result) and returns transformed text.

Common uses: summarise, translate, reformat JSON, extract structured data.

### Inputs

```json
{
  "instruction": "Summarise in 3 bullet points",
  "text":        "$fetch_result"   // or a literal string
}
```

`"text"` supports `$ref` resolution — it is resolved by `BatchExecutor` before the stage executes.

### Execution

1. Renders `transform_stage.md` with `{{INSTRUCTION}}` and `{{TEXT}}`.
2. Calls LLM (plain text mode, not JSON mode).
3. Returns the raw completion string as output.

### Output

```json
{"transformed": "• Point one\n• Point two\n• Point three"}
```

### Parallel Execution

`TransformStage` is an LLM call with no side effects on the agent queue. Multiple `TransformStage` items in the same batch with independent inputs run concurrently on the pool.

---

## ValidateStage

**File:** `stages/validate_stage.hpp` / `validate_stage.cpp`  
**Factory name:** `"ValidateStage"`  
**Prompt template:** `prompts/validate_stage.md`

### Purpose

LLM-powered validation of a previous result against a set of criteria. Optionally injects corrective work if the validation fails.

### Inputs

```json
{
  "target_id":            "write_step",
  "criteria":             "The output must be valid JSON with a 'name' field",
  "corrective_injection": true
}
```

| Field | Type | Description |
|---|---|---|
| `target_id` | string | Id of the item to validate |
| `criteria` | string | Human-readable validation criteria |
| `corrective_injection` | bool | If `true` and validation fails, ask LLM for corrective items and push them |

### Execution

1. Looks up `resultById(target_id)`.
2. Renders `validate_stage.md` with `{{OUTPUT}}`, `{{CRITERIA}}`.
3. Calls LLM; parses `{"valid": true/false, "reason": "...", "corrections": [...]}`.
4. If `valid == false` and `corrective_injection == true`: validates and pushes the `corrections` plan.

### Output

```json
{"valid": true, "reason": "Output is valid JSON with required fields"}
```
or
```json
{"valid": false, "reason": "Missing 'name' field", "corrections_injected": 1}
```

---

## Registration

Each stage provides a free registration function:

```cpp
void registerReasonStage(WorkFactory& factory);
void registerInjectionStage(WorkFactory& factory);
void registerTransformStage(WorkFactory& factory);
void registerValidateStage(WorkFactory& factory);
```

All four are called from `AgentManager::registerBuiltinItems()`.

---

## Prompt Templates

Templates live in `prompts/`. They use `{{PLACEHOLDER}}` syntax:

| Template | Key Placeholders |
|---|---|
| `reason_stage.md` | `{{TASK}}`, `{{HISTORY}}`, `{{CATALOG}}`, `{{TODO}}` |
| `injection_stage.md` | `{{PREVIOUS_OUTPUT}}`, `{{CATALOG}}`, `{{HISTORY}}` |
| `transform_stage.md` | `{{INSTRUCTION}}`, `{{TEXT}}` |
| `validate_stage.md` | `{{OUTPUT}}`, `{{CRITERIA}}` |

See [`PromptLoader`](prompt_loader.md) for the substitution engine and hot-reload mechanism.

---

## Related Components

- [`WorkItem`](work_item.md) — base class
- [`AgentContext`](agent.md) — history, LLM client, factory access
- [`PromptLoader`](prompt_loader.md) — template rendering
- [`LLMClient`](llm_client.md) — LLM call interface
- [`Actions`](actions.md) — deterministic counterparts to stages
