# Memory Actions

`src/agent/actions/memory_actions.hpp` · `src/agent/actions/memory_actions.cpp`

## Overview

The memory actions provide access to the agent's long-term memory backend (`MemoryBackend`). Three action types are provided: write, semantic search, and list. All delegate to `ctx.memory()` which is either `NoOpMemoryBackend` (default) or a real backend such as `AIModelMemoryBackend`.

---

## MemoryWriteAction

Stores a text entry in long-term memory with a unique id and optional metadata.

### Factory Registration

```
name:  "MemoryWriteAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `id` | string | Yes | Unique memory entry ID |
| `content` | string | Yes | Content to store |
| `metadata` | object | No | Arbitrary JSON metadata |

### Output

| Field | Value |
|---|---|
| `success` | `true` if written (always `true` with `NoOpMemoryBackend`) |
| `output.id` | The stored entry ID |
| `output.written` | `true` |

---

## MemoryReadAction

Performs a semantic search against stored memory entries.

### Factory Registration

```
name:  "MemoryReadAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `query` | string | Yes | — | Natural-language search query |
| `top_k` | integer | No | `5` | Maximum number of results |

### Output

| Field | Value |
|---|---|
| `success` | `true` |
| `output.entries` | JSON array of matching entries |

Each entry:

```json
{ "id": "mem1", "content": "...", "score": 0.87 }
```

Returns an empty array with `NoOpMemoryBackend`.

---

## MemoryListAction

Lists all memory entries, optionally filtered by a substring.

### Factory Registration

```
name:  "MemoryListAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `filter` | string | No | Substring filter applied to entry content or id |

### Output

| Field | Value |
|---|---|
| `success` | `true` |
| `output.entries` | JSON array with `id` and `content` fields (no score) |

---

## Backend Status

| Backend | Behaviour |
|---|---|
| `NoOpMemoryBackend` (default) | All writes are silently discarded; reads return empty |
| `AIModelMemoryBackend` | Full semantic write/search/list via `AIModel::Embed` + `AIModel::Search` |

To enable real memory: construct `AgentManager` with `std::make_shared<AIModelMemoryBackend>(model)`.

## Related Components

- [`Action`](action.md) — base class
- [`MemoryBackend`](memory_backend.md) — abstract interface these actions call
- [`AIModelMemoryBackend`](ai_model_memory_backend.md) — real semantic memory implementation
- [`AgentContext`](agent_context.md) — `ctx.memory()` accessor
