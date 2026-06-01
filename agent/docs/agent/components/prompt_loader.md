# PromptLoader

`include/agent/prompt_loader.hpp` · `src/agent/prompt_loader.cpp`

---

## Overview

`PromptLoader` loads system-prompt templates from disk, caches them, and performs `{{PLACEHOLDER}}` substitution. Every built-in stage uses it to build the LLM request string for each call.

It is shared across all agents via `AgentManager` and is protected by a `std::shared_mutex` — concurrent reads (the common case during active agent execution) are non-blocking.

---

## Construction

```cpp
explicit PromptLoader(std::filesystem::path prompts_dir);
```

Sets the directory to scan. Does not load anything yet; templates are loaded lazily on first use.

---

## Core Methods

### `load`

```cpp
std::string load(const std::string& name);
```

Loads a template by name. The filename is `<prompts_dir>/<name>.md`.

- On first call: reads from disk and stores in the cache.
- On subsequent calls: returns the cached string.
- Throws `std::runtime_error` if the file does not exist.

Acquires a `shared_lock` for cache reads; upgrades to a `unique_lock` on a cache miss before writing.

### `substitute`

```cpp
std::string substitute(const std::string& template_str,
                       const std::map<std::string, std::string>& vars);
```

Replaces every `{{KEY}}` occurrence in `template_str` with `vars.at(KEY)`.

- Throws if the template contains a `{{KEY}}` with no matching entry in `vars`.
- Extra keys in `vars` are silently ignored.
- Replacement is performed in-order; placeholders that appear multiple times are all replaced.

### `render`

```cpp
std::string render(const std::string& name,
                   const std::map<std::string, std::string>& vars);
```

Convenience: `load(name)` followed by `substitute`. This is what stages call.

```cpp
// Example from ReasonStage:
std::string prompt = ctx.promptLoader().render("reason_stage", {
    {"TASK",    task_string},
    {"HISTORY", history_json_string},
    {"CATALOG", catalog_json_string},
    {"TODO",    todo_string},
});
```

---

## Hot Reload

### `reload`

```cpp
void reload();
```

Drops the entire cache. The next `load` or `render` call re-reads templates from disk. Acquiring a `unique_lock` — briefly blocks concurrent reads.

Emits a `prompts_reloaded` event on the `EventBus` via `AgentManager::reloadPrompts`.

### `setPromptsDir`

```cpp
void setPromptsDir(std::filesystem::path dir);
```

Changes the directory and calls `reload()`. Useful for switching between prompt packs at runtime (e.g. language localisation, A/B testing).

### `promptsDir`

```cpp
std::filesystem::path promptsDir() const;
```

Returns the current directory.

---

## Template Format

Templates are Markdown files. Any `{{PLACEHOLDER}}` token is a substitution point.

Example (`reason_stage.md`):
```markdown
# Agent Task

**Task:** {{TASK}}

## Available Tools

{{CATALOG}}

## Execution History

{{HISTORY}}

## Todo List

{{TODO}}

Produce a JSON plan: `{"plan": [{"name": "...", "id": "...", "inputs": {...}}, ...]}`.
```

The LLM sees the rendered string as the system prompt.

---

## Thread-Safety

| Operation | Lock type | Notes |
|---|---|---|
| `load` (cache hit) | `shared_lock` | Fully concurrent |
| `load` (cache miss) | `unique_lock` | Briefly exclusive |
| `substitute` | None | Pure string function; no shared state |
| `render` | Same as `load` | Delegates |
| `reload` | `unique_lock` | Exclusive; clears cache |
| `setPromptsDir` | `unique_lock` | Exclusive; clears cache |

---

## C ABI Exposure

```c
am_status_t am_reload_prompts(AgentManager* mgr);
am_status_t am_set_prompts_dir(AgentManager* mgr, const char* dir_path);
```

See [C ABI](c_api.md).

---

## Related Components

- [`Stages`](stages.md) — primary consumers; call `render` on every LLM request
- [`AgentManager`](agent_manager.md) — owns the single shared `PromptLoader` instance
- [`AgentContext`](agent.md) — exposes `promptLoader()` to stages
- [`EventBus`](event_bus.md) — receives `prompts_reloaded` event after hot reload
