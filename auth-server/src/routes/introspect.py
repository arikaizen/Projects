"""Token introspection — RFC 7662.  Called by the MCP server to validate Bearer tokens."""
from datetime import datetime, timezone

from fastapi import APIRouter, Form, HTTPException, Request
from fastapi.responses import JSONResponse
from jose import JWTError

from ..jwt_utils import decode_access_token

router = APIRouter()


@router.post("/introspect")
async def introspect(
    request: Request,
    token: str = Form(...),
    token_type_hint: str = Form(default="access_token"),
):
    store = request.app.state.store

    try:
        claims = decode_access_token(token)
    except JWTError:
        return JSONResponse(content={"active": False})

    jti = claims.get("jti")
    record = store.get_token(jti) if jti else None
    if record is None or record.revoked:
        return JSONResponse(content={"active": False})
    if datetime.now(timezone.utc) > record.expires_at:
        return JSONResponse(content={"active": False})

    return JSONResponse(content={
        "active": True,
        "scope": record.scope,
        "client_id": record.client_id,
        "sub": claims.get("sub"),
        "aud": claims.get("aud"),
        "iss": claims.get("iss"),
        "exp": int(record.expires_at.timestamp()),
        "iat": int(record.issued_at.timestamp()),
        "jti": jti,
        "token_type": "Bearer",
    })
