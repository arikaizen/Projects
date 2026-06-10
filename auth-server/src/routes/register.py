"""Dynamic Client Registration — RFC 7591."""
from fastapi import APIRouter, HTTPException, Request

from ..models import ClientRegistration

router = APIRouter()


@router.post("/register", status_code=201)
async def register_client(reg: ClientRegistration, request: Request):
    store = request.app.state.store
    client = store.add_client(reg)
    return {
        "client_id": client.client_id,
        "client_name": client.client_name,
        "redirect_uris": client.redirect_uris,
        "grant_types": client.grant_types,
        "response_types": client.response_types,
        "scope": client.scope,
        "token_endpoint_auth_method": "none",
        "registration_client_uri": str(request.base_url) + f"register/{client.client_id}",
    }
