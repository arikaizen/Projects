"""OAuth 2.1 Authorization Server — main entry point."""
import os

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .jwt_utils import AUDIENCE, ISSUER, get_jwks
from .models import InMemoryStore
from .routes.authorize import router as authorize_router
from .routes.introspect import router as introspect_router
from .routes.register import router as register_router
from .routes.token import router as token_router

app = FastAPI(title="OAuth 2.1 Authorization Server", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Shared in-memory store (replace with a database-backed store for production)
app.state.store = InMemoryStore()

app.include_router(register_router)
app.include_router(authorize_router)
app.include_router(token_router)
app.include_router(introspect_router)


@app.get("/.well-known/oauth-authorization-server")
async def discovery():
    base = ISSUER.rstrip("/")
    return {
        "issuer": ISSUER,
        "authorization_endpoint": f"{base}/authorize",
        "token_endpoint": f"{base}/token",
        "introspection_endpoint": f"{base}/introspect",
        "registration_endpoint": f"{base}/register",
        "jwks_uri": f"{base}/.well-known/jwks.json",
        "response_types_supported": ["code"],
        "grant_types_supported": ["authorization_code", "refresh_token"],
        "code_challenge_methods_supported": ["S256"],
        "token_endpoint_auth_methods_supported": ["none"],
        "scopes_supported": ["tools:read", "tools:call"],
    }


@app.get("/.well-known/jwks.json")
async def jwks():
    return get_jwks()


@app.get("/health")
async def health():
    return {"status": "ok", "issuer": ISSUER, "audience": AUDIENCE}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "src.app:app",
        host=os.getenv("HOST", "0.0.0.0"),
        port=int(os.getenv("PORT", "8080")),
        reload=False,
    )
