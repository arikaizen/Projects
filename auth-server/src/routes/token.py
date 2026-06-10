"""Token endpoint — issues and refreshes access tokens."""
import secrets
from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Form, HTTPException, Request
from fastapi.responses import JSONResponse

from ..jwt_utils import (
    ACCESS_TOKEN_TTL,
    REFRESH_TOKEN_TTL,
    create_access_token,
)
from ..models import RefreshTokenRecord, TokenRecord
from ..pkce import validate_verifier_format, verify_pkce

router = APIRouter()


@router.post("/token")
async def token(
    request: Request,
    grant_type: str = Form(...),
    client_id: str = Form(...),
    redirect_uri: str = Form(default=""),
    code: str = Form(default=""),
    code_verifier: str = Form(default=""),
    refresh_token: str = Form(default=""),
    scope: str = Form(default=""),
):
    store = request.app.state.store
    client = store.get_client(client_id)
    if client is None:
        raise HTTPException(400, detail={"error": "invalid_client"})

    if grant_type == "authorization_code":
        if not code:
            raise HTTPException(400, detail={"error": "invalid_request", "error_description": "code required"})
        if not code_verifier or not validate_verifier_format(code_verifier):
            raise HTTPException(400, detail={"error": "invalid_request", "error_description": "code_verifier required and must match RFC 7636"})

        auth_code = store.consume_auth_code(code)
        if auth_code is None:
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "code not found or already used"})
        if datetime.now(timezone.utc) > auth_code.expires_at:
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "code expired"})
        if auth_code.client_id != client_id:
            raise HTTPException(400, detail={"error": "invalid_grant"})
        if redirect_uri and auth_code.redirect_uri != redirect_uri:
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "redirect_uri mismatch"})
        if not verify_pkce(code_verifier, auth_code.code_challenge, auth_code.code_challenge_method):
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "PKCE verification failed"})

        effective_scope = auth_code.scope
        jwt_str, jti, expires_at = create_access_token(client_id, effective_scope)
        store.save_token(TokenRecord(
            jti=jti, client_id=client_id, scope=effective_scope,
            issued_at=datetime.now(timezone.utc), expires_at=expires_at,
        ))

        refresh = secrets.token_urlsafe(32)
        store.save_refresh_token(RefreshTokenRecord(
            token=refresh, client_id=client_id, scope=effective_scope,
            issued_at=datetime.now(timezone.utc),
            expires_at=datetime.now(timezone.utc) + timedelta(seconds=REFRESH_TOKEN_TTL),
        ))

        return JSONResponse(content={
            "access_token": jwt_str,
            "token_type": "Bearer",
            "expires_in": ACCESS_TOKEN_TTL,
            "refresh_token": refresh,
            "scope": effective_scope,
        })

    elif grant_type == "refresh_token":
        if not refresh_token:
            raise HTTPException(400, detail={"error": "invalid_request", "error_description": "refresh_token required"})

        rt = store.consume_refresh_token(refresh_token)
        if rt is None:
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "refresh_token not found or revoked"})
        if datetime.now(timezone.utc) > rt.expires_at:
            raise HTTPException(400, detail={"error": "invalid_grant", "error_description": "refresh_token expired"})
        if rt.client_id != client_id:
            raise HTTPException(400, detail={"error": "invalid_grant"})

        effective_scope = scope if scope else rt.scope
        jwt_str, jti, expires_at = create_access_token(client_id, effective_scope)
        store.save_token(TokenRecord(
            jti=jti, client_id=client_id, scope=effective_scope,
            issued_at=datetime.now(timezone.utc), expires_at=expires_at,
        ))

        new_refresh = secrets.token_urlsafe(32)
        store.save_refresh_token(RefreshTokenRecord(
            token=new_refresh, client_id=client_id, scope=effective_scope,
            issued_at=datetime.now(timezone.utc),
            expires_at=datetime.now(timezone.utc) + timedelta(seconds=REFRESH_TOKEN_TTL),
        ))

        return JSONResponse(content={
            "access_token": jwt_str,
            "token_type": "Bearer",
            "expires_in": ACCESS_TOKEN_TTL,
            "refresh_token": new_refresh,
            "scope": effective_scope,
        })

    else:
        raise HTTPException(400, detail={"error": "unsupported_grant_type"})
