# WebSearchAction

`src/agent/actions/web_search_action.hpp` · `src/agent/actions/web_search_action.cpp`

## Overview

`WebSearchAction` is a **stub** that returns an empty results array until a search API key and provider are configured. The inputs are validated and logged, but no real network call is made.

## Factory Registration

```
name:  "WebSearchAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `query` | string | Yes | — | Search query |
| `num_results` | integer | No | `5` | Desired number of results |

## Current Behaviour (stub)

Returns:

```json
{
  "results": [],
  "query": "<the query>",
  "note": "WebSearch requires API key configuration. Set SEARCH_API_KEY in the environment and configure the search provider in AgentConfig::extra[\"search_provider\"]."
}
```

`success` is always `true` even though no results are returned, because the stub itself did not encounter an error.

## Implementing a Real Backend

To enable real search results, replace the stub body in `web_search_action.cpp` with a `WebFetchAction`-style HTTP call to your chosen search API (e.g. Serper, Bing, Google Custom Search). The inputs are already validated at this point.

## Related Components

- [`Action`](action.md) — base class
- [`WebFetchAction`](web_fetch_action.md) — real HTTP backend that can be used to call search APIs directly
