#!/usr/bin/env python3
"""Start the server stack in dependency order and stream their logs.

    1. auth server  (:8080)   — must be up first; the MCP server validates
                                tokens against it.
    2. mcp server   (:8081)   — loads mcp-server/.env for API keys + config.

The agent engine is a library (FFI), not a server, so it is not started here —
Agent Studio loads it directly. Press Ctrl+C to stop both servers cleanly.

Usage:
    python setup/run_stack.py
    python setup/run_stack.py --auth-port 8080 --mcp-port 8081
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import AUTH_SERVER_DIR, MCP_SERVER_DIR, fail, info, warn  # noqa: E402


def load_dotenv(path: Path) -> dict[str, str]:
    """Tiny .env parser (KEY=VALUE per line; ignores blanks and #comments)."""
    env: dict[str, str] = {}
    if not path.exists():
        return env
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line or "=" not in line:
            continue
        key, value = line.split("=", 1)
        env[key.strip()] = value.strip()
    return env


def wait_for_health(url: str, timeout_s: float = 15.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=2) as resp:
                if 200 <= resp.status < 300:
                    return True
        except (urllib.error.URLError, ConnectionError, OSError):
            time.sleep(0.4)
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the auth + MCP server stack.")
    parser.add_argument("--auth-port", default="8080")
    parser.add_argument("--mcp-port", default="8081")
    args = parser.parse_args()

    auth_bin = AUTH_SERVER_DIR / "build" / "auth_server"
    mcp_bin = MCP_SERVER_DIR / "build" / "mcp_server"
    if not auth_bin.exists():
        fail(f"{auth_bin} not found — run `python setup/setup_auth_server.py` first.")
    if not mcp_bin.exists():
        fail(f"{mcp_bin} not found — run `python setup/setup_mcp_server.py` first.")

    auth_origin = f"http://localhost:{args.auth_port}"
    mcp_origin = f"http://localhost:{args.mcp_port}"

    procs: list[subprocess.Popen] = []

    def shutdown(*_):
        info("Shutting down servers...")
        for p in reversed(procs):
            if p.poll() is None:
                p.terminate()
        for p in reversed(procs):
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # ── 1. Auth server ───────────────────────────────────────────────────────
    auth_env = dict(os.environ)
    auth_env.update({
        "HOST": auth_env.get("HOST", "0.0.0.0"),
        "PORT": args.auth_port,
        "AUTH_SERVER_ISSUER": auth_env.get("AUTH_SERVER_ISSUER", auth_origin),
        "MCP_SERVER_AUDIENCE": auth_env.get("MCP_SERVER_AUDIENCE", mcp_origin),
    })
    info(f"Starting auth server on {auth_origin} ...")
    procs.append(subprocess.Popen([str(auth_bin)], cwd=str(auth_bin.parent), env=auth_env))

    if wait_for_health(f"{auth_origin}/health"):
        info("Auth server is healthy.")
    else:
        warn(f"auth server didn't answer /health on {auth_origin} yet; continuing anyway.")

    # ── 2. MCP server (depends on auth) ────────────────────────────────────────
    mcp_env = dict(os.environ)
    mcp_env.update(load_dotenv(MCP_SERVER_DIR / ".env"))  # .env overrides ambient
    mcp_env.update({
        "HOST": mcp_env.get("HOST", "0.0.0.0"),
        "PORT": args.mcp_port,
        "AUTH_INTROSPECT_URL": mcp_env.get("AUTH_INTROSPECT_URL", f"{auth_origin}/introspect"),
        "MCP_SERVER_AUDIENCE": mcp_env.get("MCP_SERVER_AUDIENCE", mcp_origin),
        "MCP_REQUIRED_SCOPE": mcp_env.get("MCP_REQUIRED_SCOPE", "tools:call"),
        "TOOLS_DIR": mcp_env.get("TOOLS_DIR", str(MCP_SERVER_DIR / "tools")),
        "TOOLS_MANIFEST": mcp_env.get(
            "TOOLS_MANIFEST", str(MCP_SERVER_DIR / "tools" / "tools_manifest.json")
        ),
    })
    info(f"Starting MCP server on {mcp_origin} ...")
    procs.append(subprocess.Popen([str(mcp_bin)], cwd=str(mcp_bin.parent), env=mcp_env))

    if wait_for_health(f"{mcp_origin}/health"):
        info("MCP server is healthy.")
    else:
        warn(f"MCP server didn't answer /health on {mcp_origin} yet.")

    info("")
    info(f"Auth server: {auth_origin}   (/.well-known/oauth-authorization-server)")
    info(f"MCP server:  {mcp_origin}    (POST /mcp/v1 — needs a Bearer token)")
    info("Both running. Press Ctrl+C to stop.")

    # Wait; if either child dies, tear everything down.
    while True:
        for p in procs:
            code = p.poll()
            if code is not None:
                warn(f"a server exited (code {code}); stopping the stack.")
                shutdown()
        time.sleep(1)


if __name__ == "__main__":
    main()
