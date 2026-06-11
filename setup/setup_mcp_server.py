#!/usr/bin/env python3
"""Build the MCP server (C++) and prepare its Python tool scripts.

Produces: mcp-server/build/mcp_server  (listens on :8081)

The 60+ tool scripts under mcp-server/tools/ use only the Python standard
library, so there is nothing to pip-install for them. This script verifies a
Python 3 interpreter is available (the server shells out to it per tool call)
and seeds a .env from .env.example so you can fill in API keys.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import MCP_SERVER_DIR, cmake_build, fail, info, warn  # noqa: E402


def main() -> None:
    if not MCP_SERVER_DIR.exists():
        fail(f"mcp-server directory not found at {MCP_SERVER_DIR}")

    info("Building mcp-server...")
    build_dir = cmake_build(MCP_SERVER_DIR, target="mcp_server")

    binary = build_dir / "mcp_server"
    if not binary.exists():
        fail(f"build finished but {binary} was not produced")
    info(f"OK — mcp server binary at {binary}")

    # The server spawns `python3 tools/<name>.py` per tool call.
    if not shutil.which("python3"):
        warn("python3 not on PATH — the MCP server needs it to run tool scripts.")

    # Seed .env so the user has a place to drop API keys.
    env_example = MCP_SERVER_DIR / ".env.example"
    env_file = MCP_SERVER_DIR / ".env"
    if env_example.exists() and not env_file.exists():
        shutil.copyfile(env_example, env_file)
        info(f"Created {env_file} from .env.example — fill in API keys for the tools you use.")
    elif env_file.exists():
        info(f"{env_file} already exists — left untouched.")

    info("Run it with:  python setup/run_stack.py   (it loads mcp-server/.env automatically)")


if __name__ == "__main__":
    main()
