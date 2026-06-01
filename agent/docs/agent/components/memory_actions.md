# Memory Actions

`src/agent/actions/memory_actions.hpp` · `src/agent/actions/memory_actions.cpp`
**Kind:** Action · Registered by `registerMemoryActions(factory)`.

Three actions expose the agent's [`MemoryBackend`](memory_backend.md) (long-term memory) to plans. With the [`AIModelMemoryBackend`](ai_model_memory_backend.md) wired in, these become semantic (embedding-based) operations; with the default `NoOpMemoryBackend` they are inert.

---

## MemoryWriteAction

**Factory name:** `MemoryWriteAction`

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | string | **yes** | Unique memory entry id |
| `content` | string | **yes** | Content to store |
| `metadata` | object | no | Optional metadata |

### Output

```json
{"id": "mem_001", "written": true}
```

---

## MemoryReadAction

**Factory name:** `MemoryReadAction`

Semantic / keyword search over the backend.

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `query` | string | **yes** | Search query |
| `top_k` | integer | no (default 5) | Max results |

### Output

```json
{"entries": [{"id": "mem_001", "content": "...", "metadata": {}, "score": 0.92}]}
```

---

## MemoryListAction

**Factory name:** `MemoryListAction`

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `filter` | string | no | Optional substring filter |

### Output

```json
{"entries": [ ... ]}
```

---

## Thread-Safety

Delegates to the `MemoryBackend` implementation, which owns its synchronization (per the `memory_backend.hpp` contract). `AIModelMemoryBackend` is mutex-guarded.

## Related

- [Actions overview](actions.md) · [MemoryBackend](memory_backend.md) — the interface
- [AIModelMemoryBackend](ai_model_memory_backend.md) — real semantic implementation
- [WorkItem](work_item.md)
