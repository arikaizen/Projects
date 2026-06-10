"""Authorization endpoint — issues PKCE authorization codes."""
import secrets
from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, HTTPException, Query, Request
from fastapi.responses import RedirectResponse

from ..models import AuthCode
from ..pkce import validate_verifier_format

router = APIRouter()


@router.get("/authorize")
async def authorize(
    request: Request,
    response_type: str = Query(...),
    client_id: str = Query(...),
    redirect_uri: str = Query(...),
    scope: str = Query("tools:read tools:call"),
    state: str = Query(default=""),
    code_challenge: str = Query(...),
    code_challenge_method: str = Query("S256"),
):
    store = request.app.state.store

    if response_type != "code":
        raise HTTPException(400, "unsupported_response_type")
    if code_challenge_method != "S256":
        raise HTTPException(400, "unsupported_code_challenge_method — only S256 accepted")

    client = store.get_client(client_id)
    if client is None:
        raise HTTPException(400, "invalid_client")
    if redirect_uri not in client.redirect_uris:
        raise HTTPException(400, "invalid_redirect_uri")

    # Minimal scope validation — only allow subsets of registered scope
    requested = set(scope.split())
    allowed = set(client.scope.split())
    if not requested.issubset(allowed):
        raise HTTPException(400, "invalid_scope")

    code = secrets.token_urlsafe(32)
    auth_code = AuthCode(
        code=code,
        client_id=client_id,
        redirect_uri=redirect_uri,
        scope=scope,
        code_challenge=code_challenge,
        code_challenge_method=code_challenge_method,
        expires_at=datetime.now(timezone.utc) + timedelta(minutes=10),
    )
    store.save_auth_code(auth_code)

    sep = "&" if "?" in redirect_uri else "?"
    location = f"{redirect_uri}{sep}code={code}"
    if state:
        location += f"&state={state}"
    return RedirectResponse(url=location, status_code=302)
