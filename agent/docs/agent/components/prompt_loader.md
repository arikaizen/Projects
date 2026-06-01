# PromptLoader

`include/agent/prompt_loader.hpp` · `src/agent/prompt_loader.cpp`

## Overview

`PromptLoader` loads system-prompt Markdown templates from disk, caches them in memory, and performs `{{PLACEHOLDER}}` substitution. Every built-in stage uses this component to render its per-call prompt.

## Construction

```cpp
explicit PromptLoader(std::filesystem::path prompts_dir);
```

## Interface

### `load`

```cpp
std::string load(const std::string& name);
```

Loads the template file `<prompts_dir>/<name>.md`. The result is cached after first load. Throws `std::runtime_error` if the file does not exist.

### `reload`

```cpp
void reload();
```

Clears the cache. The next `load()` call re-reads from disk. Used by `AgentManager::reloadPrompts()` and `am_reload_prompts()`.

### `substitute`

```cpp
std::string substitute(const std::string& template_str,
                       const std::map<std::string, std::string>& vars);
```

Replaces every `{{KEY}}` occurrence in `template_str` with `vars.at(KEY)`. Throws if the template contains a placeholder with no matching key in `vars`. Extra keys in `vars` are silently ignored.

### `render`

```cpp
std::string render(const std::string& name,
                   const std::map<std::string, std::string>& vars);
```

Convenience: `load(name)` + `substitute(template, vars)`. This is what all four stages call.

### `setPromptsDir`

```cpp
void setPromptsDir(std::filesystem::path dir);
```

Changes the template directory and clears the cache. Called by `AgentManager::setPromptsDir`.

## Template Files

Each built-in stage requires a corresponding `.md` file:

| Stage | Template file |
|---|---|
| `ReasonStage` | `reason_stage.md` |
| `InjectionStage` | `injection_stage.md` |
| `TransformStage` | `transform_stage.md` |
| `ValidateStage` | `validate_stage.md` (+ optional `validate_stage_corrective`) |

Missing files cause a `std::runtime_error` at render time. Template placeholders are documented in [stages.md](stages.md).

## Thread-Safety

`PromptLoader` uses a `std::shared_mutex`:
- `load`, `render`, `substitute` acquire a `shared_lock` (concurrent reads from the cache).
- `reload`, `setPromptsDir` acquire a `unique_lock` (exclusive write to clear the cache).

## Hot Reload

During development, templates can be edited without restarting the process. Call `AgentManager::reloadPrompts()` (or `am_reload_prompts()` via the C ABI) after editing. The next agent run will pick up the new template text.

## Related Components

- [`ReasonStage`](reason_stage.md), [`InjectionStage`](injection_stage.md), [`TransformStage`](transform_stage.md), [`ValidateStage`](validate_stage.md) — all call `ctx.promptLoader().render(...)`
- [`AgentManager`](agent_manager.md) — owns the `PromptLoader`; exposes hot-reload API
- [`AgentContext`](agent_context.md) — holds a `shared_ptr<PromptLoader>`
