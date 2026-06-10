"""Tools that proxy to the SIEM Flask service (siem/main, port 5000).

The MCP server's own service-account credentials are used here — the
user's raw SIEM keys are never exposed to the model (step ⑤–⑥).
"""
import os
from typing import Any

import httpx

_SIEM_BASE = os.getenv("SIEM_BASE_URL", "http://localhost:5000")
_SIEM_API_KEY = os.getenv("SIEM_API_KEY", "")  # server-side credential
_TIMEOUT = 10.0


def _headers() -> dict:
    h = {"Content-Type": "application/json"}
    if _SIEM_API_KEY:
        h["X-API-Key"] = _SIEM_API_KEY
    return h


async def siem_get_status(args: dict) -> Any:
    """Return SIEM system status."""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.get(f"{_SIEM_BASE}/api/status", headers=_headers())
        resp.raise_for_status()
    return resp.json()


async def siem_search_logs(args: dict) -> Any:
    """Search SIEM logs.  args: {query, limit}"""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.post(
            f"{_SIEM_BASE}/api/data",
            json={"query": args.get("query", ""), "limit": args.get("limit", 100)},
            headers=_headers(),
        )
        resp.raise_for_status()
    return resp.json()


async def siem_ingest_event(args: dict) -> Any:
    """Ingest a log event into the SIEM.  args: {event}"""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.post(
            f"{_SIEM_BASE}/api/data",
            json=args.get("event", {}),
            headers=_headers(),
        )
        resp.raise_for_status()
    return resp.json()


TOOL_DEFINITIONS = [
    {
        "name": "siem_get_status",
        "description": "Return the SIEM system health and statistics.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "siem_search_logs",
        "description": "Search the SIEM log store.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "Search query string"},
                "limit": {"type": "integer", "default": 100},
            },
        },
    },
    {
        "name": "siem_ingest_event",
        "description": "Ingest a structured log event into the SIEM.",
        "inputSchema": {
            "type": "object",
            "required": ["event"],
            "properties": {"event": {"type": "object"}},
        },
    },
]

HANDLERS = {
    "siem_get_status": siem_get_status,
    "siem_search_logs": siem_search_logs,
    "siem_ingest_event": siem_ingest_event,
}
