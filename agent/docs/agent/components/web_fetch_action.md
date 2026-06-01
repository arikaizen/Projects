# WebFetchAction

`src/agent/actions/web_fetch_action.hpp` · `src/agent/actions/web_fetch_action.cpp`
**Factory name:** `WebFetchAction` · **Kind:** Action

---

## Purpose

Fetches a URL over HTTP/HTTPS. Uses `cpp-httplib` (`httplib.h`) when available at compile time; otherwise falls back to `curl` via `popen`. This action does real network I/O (it is not a stub).

## Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `url` | string | **yes** | URL to fetch |
| `method` | string | no (default `GET`) | HTTP method |
| `headers` | object | no | Optional HTTP headers |
| `body` | string | no | Optional request body |

## Output

```json
{"status_code": 200, "body": "<response body>", "url": "<url>"}
```

## Implementation Note

`curlFetch(url, method, body, headers)` is the fallback path returning `{status_code, body}`.

## Thread-Safety

Each call is independent with no shared state; fully concurrent.

## Related

- [Actions overview](actions.md) · [WebSearchAction](web_search_action.md) · [WorkItem](work_item.md)
