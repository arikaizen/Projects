"""JSON-RPC 2.0 dispatcher for the MCP protocol."""
from __future__ import annotations

import traceback
from typing import Any, Callable, Awaitable

from fastapi import HTTPException


_PARSE_ERROR = -32700
_INVALID_REQUEST = -32600
_METHOD_NOT_FOUND = -32601
_INVALID_PARAMS = -32602
_INTERNAL_ERROR = -32603

ToolHandler = Callable[[dict], Awaitable[Any]]


class JsonRpcDispatcher:
    def __init__(self) -> None:
        self._methods: dict[str, ToolHandler] = {}

    def register(self, name: str, handler: ToolHandler) -> None:
        self._methods[name] = handler

    def list_methods(self) -> list[str]:
        return list(self._methods.keys())

    async def dispatch(self, body: dict) -> dict:
        req_id = body.get("id")

        if body.get("jsonrpc") != "2.0":
            return _error_response(req_id, _INVALID_REQUEST, "jsonrpc must be '2.0'")

        method = body.get("method")
        if not isinstance(method, str):
            return _error_response(req_id, _INVALID_REQUEST, "method must be a string")

        params = body.get("params", {})

        # MCP protocol methods
        if method == "tools/list":
            return _ok(req_id, {"tools": await self._tools_list()})

        if method == "tools/call":
            return await self._tools_call(req_id, params)

        if method == "ping":
            return _ok(req_id, {"pong": True})

        if method in self._methods:
            try:
                result = await self._methods[method](params)
                return _ok(req_id, result)
            except Exception as exc:
                return _error_response(req_id, _INTERNAL_ERROR, str(exc))

        return _error_response(req_id, _METHOD_NOT_FOUND, f"method not found: {method}")

    async def _tools_list(self) -> list[dict]:
        from .tools.registry import TOOL_SCHEMAS
        return TOOL_SCHEMAS

    async def _tools_call(self, req_id: Any, params: dict) -> dict:
        tool_name = params.get("name")
        arguments = params.get("arguments", {})

        if not tool_name:
            return _error_response(req_id, _INVALID_PARAMS, "params.name required")

        handler = self._methods.get(tool_name)
        if handler is None:
            return _error_response(req_id, _METHOD_NOT_FOUND, f"tool not found: {tool_name}")

        try:
            result = await handler(arguments)
            return _ok(req_id, {"content": [{"type": "text", "text": str(result)}]})
        except Exception as exc:
            return _error_response(req_id, _INTERNAL_ERROR, str(exc))


def _ok(req_id: Any, result: Any) -> dict:
    return {"jsonrpc": "2.0", "id": req_id, "result": result}


def _error_response(req_id: Any, code: int, message: str) -> dict:
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": code, "message": message},
    }
