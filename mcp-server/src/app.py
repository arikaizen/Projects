"""MCP Server — main entry point.

Exposes a Streamable HTTP transport (POST /mcp/v1) protected by Bearer
token validation.  A separate stdio transport is available for local
subprocess use (python -m mcp_server.stdio).
"""
import os

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .jsonrpc import JsonRpcDispatcher
from .tools.registry import ALL_HANDLERS
from .transport.http import make_http_router

app = FastAPI(title="MCP Server", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["POST", "GET", "OPTIONS"],
    allow_headers=["Authorization", "Content-Type"],
)

# Build the dispatcher and register all tools
dispatcher = JsonRpcDispatcher()
for name, handler in ALL_HANDLERS.items():
    dispatcher.register(name, handler)

app.include_router(make_http_router(dispatcher))


@app.get("/health")
async def health():
    return {
        "status": "ok",
        "tools": dispatcher.list_methods(),
        "auth_introspect_url": os.getenv("AUTH_INTROSPECT_URL", "http://localhost:8080/introspect"),
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "src.app:app",
        host=os.getenv("HOST", "0.0.0.0"),
        port=int(os.getenv("PORT", "8081")),
        reload=False,
    )
