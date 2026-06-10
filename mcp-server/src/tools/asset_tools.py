"""Tools that proxy to the asset-map FastAPI service (asset map/, port 8000).

Server-side credentials only — the model never sees raw asset-map keys.
"""
import os
from typing import Any

import httpx

_ASSET_BASE = os.getenv("ASSET_MAP_BASE_URL", "http://localhost:8000")
_ASSET_API_KEY = os.getenv("ASSET_MAP_API_KEY", "")
_TIMEOUT = 10.0


def _headers() -> dict:
    h = {"Content-Type": "application/json"}
    if _ASSET_API_KEY:
        h["X-API-Key"] = _ASSET_API_KEY
    return h


async def asset_get_markers(args: dict) -> Any:
    """Return all asset map markers."""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.get(f"{_ASSET_BASE}/api/data", headers=_headers())
        resp.raise_for_status()
    return resp.json()


async def asset_add_marker(args: dict) -> Any:
    """Add a marker to the asset map.  args: {name, lat, lon, type, metadata}"""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.post(
            f"{_ASSET_BASE}/api/data",
            json={
                "name": args.get("name", ""),
                "lat": args["lat"],
                "lon": args["lon"],
                "type": args.get("type", "generic"),
                "metadata": args.get("metadata", {}),
            },
            headers=_headers(),
        )
        resp.raise_for_status()
    return resp.json()


async def asset_get_status(args: dict) -> Any:
    """Return asset-map service status."""
    async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
        resp = await client.get(f"{_ASSET_BASE}/api/status", headers=_headers())
        resp.raise_for_status()
    return resp.json()


TOOL_DEFINITIONS = [
    {
        "name": "asset_get_markers",
        "description": "Return all markers currently stored in the asset map.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "asset_add_marker",
        "description": "Add a new marker to the asset map.",
        "inputSchema": {
            "type": "object",
            "required": ["lat", "lon"],
            "properties": {
                "name": {"type": "string"},
                "lat": {"type": "number"},
                "lon": {"type": "number"},
                "type": {"type": "string"},
                "metadata": {"type": "object"},
            },
        },
    },
    {
        "name": "asset_get_status",
        "description": "Return asset-map service health.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]

HANDLERS = {
    "asset_get_markers": asset_get_markers,
    "asset_add_marker": asset_add_marker,
    "asset_get_status": asset_get_status,
}
