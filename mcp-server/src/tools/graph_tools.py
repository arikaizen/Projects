"""Tools that proxy to the C++ graph database server (data server/, port 7474).

The MCP server uses its own scoped service credentials — the user's raw
secrets are never forwarded to the model (step ⑤–⑥).
"""
import json
import os
import socket
from typing import Any


_GRAPH_HOST = os.getenv("GRAPH_DB_HOST", "localhost")
_GRAPH_PORT = int(os.getenv("GRAPH_DB_PORT", "7474"))
_REQUEST_TIMEOUT = 5.0


def _send_request(payload: dict) -> dict:
    """Send a JSON request to the graph server over TCP and read the response."""
    raw = (json.dumps(payload) + "\n").encode()
    with socket.create_connection((_GRAPH_HOST, _GRAPH_PORT),
                                  timeout=_REQUEST_TIMEOUT) as sock:
        sock.sendall(raw)
        buf = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                break
    return json.loads(buf.split(b"\n")[0])


async def graph_add_node(args: dict) -> Any:
    """Add a node to the graph.  args: {id, labels, properties}"""
    resp = _send_request({
        "method": "graph.addNode",
        "params": {
            "id": args["id"],
            "labels": args.get("labels", []),
            "properties": args.get("properties", {}),
        },
    })
    return resp


async def graph_add_edge(args: dict) -> Any:
    """Add a directed edge.  args: {from_id, to_id, type, properties}"""
    resp = _send_request({
        "method": "graph.addEdge",
        "params": {
            "from": args["from_id"],
            "to": args["to_id"],
            "type": args.get("type", "RELATED"),
            "properties": args.get("properties", {}),
        },
    })
    return resp


async def graph_query(args: dict) -> Any:
    """Run a graph query.  args: {query}"""
    resp = _send_request({
        "method": "graph.query",
        "params": {"query": args.get("query", "")},
    })
    return resp


async def graph_get_node(args: dict) -> Any:
    """Retrieve a node by id.  args: {id}"""
    resp = _send_request({
        "method": "graph.getNode",
        "params": {"id": args["id"]},
    })
    return resp


TOOL_DEFINITIONS = [
    {
        "name": "graph_add_node",
        "description": "Add a node to the graph database.",
        "inputSchema": {
            "type": "object",
            "required": ["id"],
            "properties": {
                "id": {"type": "string"},
                "labels": {"type": "array", "items": {"type": "string"}},
                "properties": {"type": "object"},
            },
        },
    },
    {
        "name": "graph_add_edge",
        "description": "Add a directed edge between two graph nodes.",
        "inputSchema": {
            "type": "object",
            "required": ["from_id", "to_id"],
            "properties": {
                "from_id": {"type": "string"},
                "to_id": {"type": "string"},
                "type": {"type": "string"},
                "properties": {"type": "object"},
            },
        },
    },
    {
        "name": "graph_query",
        "description": "Execute a query against the graph database.",
        "inputSchema": {
            "type": "object",
            "required": ["query"],
            "properties": {"query": {"type": "string"}},
        },
    },
    {
        "name": "graph_get_node",
        "description": "Retrieve a node by its ID.",
        "inputSchema": {
            "type": "object",
            "required": ["id"],
            "properties": {"id": {"type": "string"}},
        },
    },
]

HANDLERS = {
    "graph_add_node": graph_add_node,
    "graph_add_edge": graph_add_edge,
    "graph_query": graph_query,
    "graph_get_node": graph_get_node,
}
