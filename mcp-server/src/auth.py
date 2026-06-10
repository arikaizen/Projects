"""Bearer token validation middleware — calls auth server introspection (step ④)."""
import os
from typing import Optional

import httpx
from fastapi import HTTPException, Request
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

_INTROSPECT_URL = os.getenv("AUTH_INTROSPECT_URL", "http://localhost:8080/introspect")
_REQUIRED_AUDIENCE = os.getenv("MCP_SERVER_AUDIENCE", "http://localhost:8081")
_REQUIRED_SCOPE = os.getenv("MCP_REQUIRED_SCOPE", "tools:call")

_bearer_scheme = HTTPBearer(auto_error=False)


async def validate_bearer_token(request: Request) -> dict:
    """Extract Bearer token, call auth server introspection, return active claims."""
    auth_header: Optional[str] = request.headers.get("Authorization")
    if not auth_header or not auth_header.lower().startswith("bearer "):
        raise HTTPException(status_code=401, detail="Bearer token required",
                            headers={"WWW-Authenticate": "Bearer"})

    token = auth_header[7:].strip()

    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            resp = await client.post(
                _INTROSPECT_URL,
                data={"token": token, "token_type_hint": "access_token"},
                headers={"Content-Type": "application/x-www-form-urlencoded"},
            )
        resp.raise_for_status()
        claims = resp.json()
    except httpx.HTTPError as exc:
        raise HTTPException(status_code=503,
                            detail=f"Auth server unreachable: {exc}") from exc

    if not claims.get("active"):
        raise HTTPException(status_code=401, detail="Token inactive or expired",
                            headers={"WWW-Authenticate": "Bearer error=\"invalid_token\""})

    # ④ Validate audience and scope
    aud = claims.get("aud")
    if isinstance(aud, list):
        if _REQUIRED_AUDIENCE not in aud:
            raise HTTPException(status_code=403, detail="Token audience mismatch")
    elif aud != _REQUIRED_AUDIENCE:
        raise HTTPException(status_code=403, detail="Token audience mismatch")

    token_scopes = set(claims.get("scope", "").split())
    required_scopes = set(_REQUIRED_SCOPE.split())
    if not required_scopes.issubset(token_scopes):
        missing = required_scopes - token_scopes
        raise HTTPException(status_code=403,
                            detail=f"Insufficient scope — missing: {', '.join(missing)}")

    return claims
