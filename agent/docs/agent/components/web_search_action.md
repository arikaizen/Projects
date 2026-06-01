# WebSearchAction

`src/agent/actions/web_search_action.hpp` · `src/agent/actions/web_search_action.cpp`
**Factory name:** `WebSearchAction` · **Kind:** Action

---

## Purpose

**Stub** web search. As shipped it returns a placeholder response indicating that a search API key must be configured. A real implementation would call a search API (SerpAPI, Brave Search, etc.) using a key from `AgentContext::config().extra` or the environment.

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `query` | string | **yes** | Search query |
| `num_results` | integer | no (default 5) | Desired number of results |

## Output

A placeholder result object noting that an API key is required (no live results until wired up).

## Thread-Safety

No shared mutable state; fully concurrent.

## Status

⚠️ Stub — see the stub inventory in [Actions overview](actions.md).

## Related

- [Actions overview](actions.md) · [WebFetchAction](web_fetch_action.md) · [WorkItem](work_item.md)
