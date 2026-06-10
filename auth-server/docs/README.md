# Auth Server

OAuth 2.1 Authorization Server implemented in **C++17** using `cpp-httplib`, `nlohmann/json`, and OpenSSL.

## Architecture

```
Client (Flutter / agent)
    │
    ├─①─ POST /register            ← Dynamic client registration (RFC 7591)
    ├─①─ GET  /authorize?...       ← PKCE S256 authorization code request
    ├─②─ POST /token               ← Exchange code for access + refresh tokens
    │
MCP Server
    └─④─ POST /introspect          ← Token validation (audience, scope, expiry)
```

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/register` | Register a new OAuth client (RFC 7591 dynamic registration) |
| `GET`  | `/authorize` | Authorization endpoint — issues PKCE authorization code |
| `POST` | `/token` | Token endpoint — `authorization_code` and `refresh_token` grants |
| `POST` | `/introspect` | Token introspection (RFC 7662) — used by the MCP server |
| `GET`  | `/.well-known/oauth-authorization-server` | Server discovery document |
| `GET`  | `/.well-known/jwks.json` | Public key set (RSA-2048, RS256) |
| `GET`  | `/health` | Health check |

## Security Design

- **RS256 JWT** — An RSA-2048 key pair is generated on every startup. The public key is published at `/jwks.json`. Tokens are signed with the private key; the MCP server verifies them via introspection.
- **PKCE S256** — Clients must supply a `code_challenge` (SHA-256 of the verifier, base64url-encoded). The verifier is validated at the token endpoint.
- **Short-lived access tokens** — Default 1 hour (configurable via `ACCESS_TOKEN_TTL`).
- **Refresh tokens** — Single-use; a new refresh token is issued on every refresh.
- **No client secrets** — Public clients only (`token_endpoint_auth_method: none`).
- **Audience validation** — The `aud` claim is set to `MCP_SERVER_AUDIENCE`; the introspection endpoint checks it.

## Building

### Prerequisites

- CMake ≥ 3.17
- C++17 compiler (GCC 9+, Clang 10+)
- OpenSSL ≥ 1.1.1
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only)
- nlohmann/json (auto-fetched by CMake if not installed)

```bash
mkdir build && cd build
cmake .. -DHTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib
make -j$(nproc)
```

## Configuration

Copy `.env.example` to `.env` and set the values:

| Variable | Default | Description |
|----------|---------|-------------|
| `HOST` | `0.0.0.0` | Bind address |
| `PORT` | `8080` | Listen port |
| `AUTH_SERVER_ISSUER` | `http://localhost:8080` | Issuer URL embedded in JWT `iss` claim |
| `MCP_SERVER_AUDIENCE` | `http://localhost:8081` | Audience embedded in JWT `aud` claim |
| `ACCESS_TOKEN_TTL` | `3600` | Access token lifetime (seconds) |
| `REFRESH_TOKEN_TTL` | `86400` | Refresh token lifetime (seconds) |

## OAuth 2.1 Flow

```
1. Client  → POST /register           → receives client_id
2. Client  → GET  /authorize?...      → browser redirects to redirect_uri?code=...
3. Client  → POST /token (code+verifier) → receives access_token + refresh_token
4. MCP Server → POST /introspect (Bearer) → receives active/inactive + claims
```

## In-Memory Store

All state (clients, auth codes, tokens) is held in memory. For production:
- Replace `Store` with a Redis or PostgreSQL-backed implementation.
- Persist the RSA key pair across restarts (or use an HSM).
- Add token revocation storage if needed.
