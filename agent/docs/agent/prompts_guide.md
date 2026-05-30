# Prompt Templates Guide

This guide covers how to write and maintain system prompt templates for the
agent engine's built-in stages.

---

## Placeholder Syntax

All templates use `{{KEY}}` placeholders — a double-brace-wrapped uppercase
identifier. The `PromptLoader::substitute()` method replaces every occurrence
of `{{KEY}}` with the corresponding value from the `vars` map.

Rules:
- Keys are **case-sensitive** and **uppercase** by convention.
- The same placeholder may appear multiple times; all occurrences are replaced.
- Extra keys in `vars` are silently ignored.
- A placeholder with **no matching key** in `vars` causes `substitute()` to
  throw `std::runtime_error`. Ensure every placeholder in your template has a
  corresponding entry in the vars map when calling `render()`.

---

## Per-Stage Placeholder Contracts

### `reason_stage.md` — Primary reasoning (Reading 1)

Called once per agent iteration. The LLM reads the full agent state and
produces a plan (JSON array of work items).

| Placeholder        | Content                                               |
|--------------------|-------------------------------------------------------|
| `{{CATALOG}}`      | JSON listing all registered `WorkItem` types + schemas |
| `{{HISTORY}}`      | JSON summary of the last N executed items             |
| `{{QUEUE}}`        | JSON summary of items currently in the queue          |
| `{{TASK}}`         | The agent's current task string                       |
| `{{OUTPUT_SCHEMA}}`| JSON Schema for the expected LLM response format      |

**Expected LLM output:** a JSON array where each element has `name`, `id`,
`inputs` fields. Optionally the last element has `final_answer` to terminate
the agent.

### `injection_stage.md` — Meta-stage (Reading 2)

Called after a specific item completes. The LLM examines that item's output and
decides what to inject next.

| Placeholder           | Content                                            |
|-----------------------|----------------------------------------------------|
| `{{CATALOG}}`         | Registered `WorkItem` types                        |
| `{{HISTORY}}`         | Recent execution history                           |
| `{{QUEUE}}`           | Current queue                                      |
| `{{TASK}}`            | Agent task string                                  |
| `{{PREVIOUS_RESULT}}` | JSON output of the target item (or last result)    |
| `{{OUTPUT_SCHEMA}}`   | JSON Schema for the expected response format       |

**Expected LLM output:** JSON array of items to inject at the **front** of the
queue (so they execute before any already-queued items).

### `transform_stage.md` — Text transformation

A simple two-field template for applying a transformation to a piece of text.

| Placeholder        | Content                                             |
|--------------------|-----------------------------------------------------|
| `{{INSTRUCTION}}`  | What transformation to perform                      |
| `{{INPUT_TEXT}}`   | The text to transform                               |

**Expected LLM output:** the transformed text (not necessarily JSON).

### `validate_stage.md` — Output validation

Checks whether a produced output meets defined criteria.

| Placeholder         | Content                                           |
|---------------------|---------------------------------------------------|
| `{{TARGET_OUTPUT}}` | The output to be validated                        |
| `{{CRITERIA}}`      | The validation criteria (as text or JSON)         |

**Expected LLM output:** a JSON object with at least `{"pass": true|false,
"reason": "..."}`.

---

## Writing Effective Templates

### Clear role definition

Open with a one-sentence role statement. The LLM performs better when it knows
its exact purpose.

```markdown
You are an autonomous agent executing a specific task. Your job is to plan
the next steps given the current state.
```

### Structured output section

Always conclude with a clearly delimited output section. Placing it last
encourages the LLM to reason first and format second.

```markdown
## Your Response

Respond with **only** a JSON array, no other text. Each element:

```json
{"name": "ActionName", "id": "unique_id", "inputs": {...}}
```

Include `"final_answer": "..."` on the last item when the task is complete.
```

### Concrete examples

Include at least one worked example in the template. This dramatically reduces
format errors.

```markdown
## Example Plan

```json
[
  {"name": "ReadAction", "id": "r1", "inputs": {"path": "/tmp/report.txt"}},
  {"name": "EchoAction", "id": "e1", "inputs": {"summary": "$r1.content"}}
]
```
```

### Limit history depth

The `{{HISTORY}}` placeholder is populated by `ctx.historySummaryJson(20)`.
If your agent accumulates very long history, reduce `max_entries` in the
`historySummaryJson()` call to stay within the LLM's context window.

---

## Adding a New Stage with a New Template

1. **Create the template file** in the `prompts_dir`:
   ```
   prompts/my_stage.md
   ```

2. **Define the placeholder contract** (which keys your stage's `render()` call
   will provide).

3. **Implement the Stage class** calling:
   ```cpp
   std::string prompt = ctx.promptLoader().render("my_stage", {
       {"TASK",        ctx.config().task},
       {"MY_CUSTOM",   some_value},
       // … all keys used in the template …
   });
   ```

4. **Register with WorkFactory** via `registerItem()` (see `README.md` §6).

5. **Write a stub template** for tests so `PromptLoader` does not throw:
   ```cpp
   std::ofstream f("/tmp/test_prompts/my_stage.md");
   f << "TASK:{{TASK}}\nCUSTOM:{{MY_CUSTOM}}\n";
   ```

---

## Example: Well-Written `reason_stage.md`

```markdown
# Agent Reasoning Stage

You are a task-completion agent. Your objective is:

**{{TASK}}**

---

## Available Actions

The following work items are registered and available for you to use:

{{CATALOG}}

---

## Execution History

Items already completed (most recent last):

{{HISTORY}}

---

## Current Queue

Items already scheduled but not yet executed:

{{QUEUE}}

---

## Instructions

1. Read the history to understand what has been done.
2. Choose the minimal set of next actions to make progress on the task.
3. If the task is fully complete, set `final_answer` on the last item.
4. Do NOT re-do actions that appear in history.
5. Use `$id.field` syntax in `inputs` to reference earlier results.

---

## Output Format

Respond with **only** a JSON array (no prose, no markdown fences):

{{OUTPUT_SCHEMA}}

### Example

```json
[
  {"name": "ReadAction",  "id": "r1", "inputs": {"path": "/etc/hosts"}},
  {"name": "GrepAction",  "id": "g1", "inputs": {"pattern": "localhost", "text": "$r1.content"}},
  {"name": "EchoAction",  "id": "e1", "inputs": {"result": "$g1.matches"}, "final_answer": "done"}
]
```
```

---

## Tips for Multi-Stage Agents

- **Keep templates focused.** Each stage should do one thing: plan, inject,
  transform, or validate.
- **Test with stubs.** All tests use minimal stub templates so `PromptLoader`
  doesn't throw. Real templates are only loaded in production or integration tests.
- **Version your templates.** Keep templates under source control alongside the
  stage code that renders them. A template change should be reviewed alongside
  the stage's `vars` map to ensure no placeholder is dropped.
- **Hot reload in production.** Call `mgr.reloadPrompts()` (or
  `am_reload_prompts(mgr)`) after updating template files on disk to pick up
  changes without restarting the process.
