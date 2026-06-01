# WebFetchAction

`src/agent/actions/web_fetch_action.hpp` · `src/agent/actions/web_fetch_action.cpp`

## Overview

`WebFetchAction` performs an HTTP or HTTPS request and returns the status code and response body. It uses **cpp-httplib** (`httplib.h`) when available at compile time, otherwise falls back to invoking `curl` as a subprocess.

## Factory Registration

```
name:  "WebFetchAction"
kind:  Action
```

**Input schema:**

| Input | Type | Required | Default | Description |
|---|---|---|---|---|
| `url` | string | Yes | — | URL to fetch |
| `method` | string | No | `"GET"` | HTTP method (`GET`, `POST`, `PUT`, `DELETE`) |
| `headers` | object | No | `{}` | HTTP headers as a key-value JSON object |
| `body` | string | No | `""` | Request body (for POST/PUT) |

## Execution

With httplib:
1. Parses the URL into scheme, host, and path.
2. Constructs an `httplib::Client` (or `httplib::SSLClient` for `https://`).
3. Builds headers and dispatches the request based on `method`.

Without httplib (curl fallback):
1. Builds a `curl -s -L -X METHOD` command.
2. Adds headers with `-H`, body via `printf '%s' ... | curl ... --data-binary @-`.
3. Appends `-w '\n__STATUS__%{http_code}'` to capture the status code.

## Output

| Field | Value |
|---|---|
| `success` | `true` when HTTP status is `2xx` |
| `output.status_code` | HTTP status code (`-1` on network error) |
| `output.body` | Response body as a string |
| `output.url` | The requested URL |
| `error` | `"HTTP NNN"` when status is not `2xx` |

## Example

```json
{ "name": "WebFetchAction", "id": "f1",
  "inputs": { "url": "https://api.example.com/data" } }
```

POST with headers and body:

```json
{ "name": "WebFetchAction", "id": "f2",
  "inputs": {
    "url": "https://api.example.com/submit",
    "method": "POST",
    "headers": { "Content-Type": "application/json", "Authorization": "Bearer TOKEN" },
    "body": "{\"key\": \"value\"}"
  }
}
```

## Notes

- SSL certificate verification is disabled in the httplib path (`enable_server_certificate_verification(false)`). Enable it for production use.
- The curl fallback uses single-quote shell escaping; URLs or headers with embedded single quotes may cause issues.

## Related Components

- [`Action`](action.md) — base class
- [`WebSearchAction`](web_search_action.md) — higher-level search (stub)
