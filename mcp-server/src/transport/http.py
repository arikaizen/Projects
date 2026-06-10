"""Streamable HTTP transport — POST /mcp/v1 with Bearer auth (step ③).

The endpoint accepts a single JSON-RPC 2.0 request or a JSON array of
requests (batch).  Responses are returned as application/json.
"""
from fastapi import APIRouter, Depends, Request
from fastapi.responses import JSONResponse

from ..auth import validate_bearer_token
from ..jsonrpc import JsonRpcDispatcher

router = APIRouter()


def make_http_router(dispatcher: JsonRpcDispatcher) -> APIRouter:
    @router.post("/mcp/v1")
    async def mcp_endpoint(
        request: Request,
        claims: dict = Depends(validate_bearer_token),
    ):
        try:
            body = await request.json()
        except Exception:
            return JSONResponse(
                status_code=400,
                content={"jsonrpc": "2.0", "id": None,
                         "error": {"code": -32700, "message": "Parse error"}},
            )

        if isinstance(body, list):
            results = [await dispatcher.dispatch(item) for item in body]
            return JSONResponse(content=results)

        result = await dispatcher.dispatch(body)
        return JSONResponse(content=result)

    return router
