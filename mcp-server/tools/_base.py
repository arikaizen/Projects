"""Base utility for all MCP tool scripts.

Each tool script reads one JSON line from stdin:
    {"args": {...}, "api_keys": {...}}
and writes one JSON line to stdout:
    {"success": true, "result": ...}
 or {"success": false, "error": "message"}
"""
import json
import sys
import traceback


def read_input():
    line = sys.stdin.readline().strip()
    if not line:
        raise ValueError("Empty input")
    doc = json.loads(line)
    return doc.get("args", {}), doc.get("api_keys", {})


def success(result):
    print(json.dumps({"success": True, "result": result}), flush=True)
    sys.exit(0)


def failure(error: str):
    print(json.dumps({"success": False, "error": error}), flush=True)
    sys.exit(0)


def run(handler):
    """Wraps a handler(args, api_keys) -> result call with error handling."""
    try:
        args, api_keys = read_input()
        result = handler(args, api_keys)
        success(result)
    except Exception:
        failure(traceback.format_exc())
