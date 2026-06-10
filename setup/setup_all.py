#!/usr/bin/env python3
"""Build the whole stack in dependency order.

    1. auth server   (C++  -> auth-server/build/auth_server,  :8080)
    2. mcp server     (C++  -> mcp-server/build/mcp_server,    :8081)
    3. agent engine   (C++  -> agent/build/libagent_engine.so, FFI lib)
    4. agent studio   (Flutter `pub get`; skipped if Flutter is absent)

cmake is installed automatically (via pip) if you don't have it.

Usage:
    python setup/setup_all.py             # build everything
    python setup/setup_all.py --no-studio # skip the Flutter step
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import ensure_cmake, info  # noqa: E402
import setup_agent_engine  # noqa: E402
import setup_agent_studio  # noqa: E402
import setup_auth_server  # noqa: E402
import setup_mcp_server  # noqa: E402


def main() -> None:
    skip_studio = "--no-studio" in sys.argv

    info("=== Bootstrapping toolchain ===")
    ensure_cmake()  # install cmake once up front so all builds reuse it

    info("=== 1/4  Auth server ===")
    setup_auth_server.main()

    info("=== 2/4  MCP server ===")
    setup_mcp_server.main()

    info("=== 3/4  Agent engine ===")
    setup_agent_engine.main()

    if skip_studio:
        info("=== 4/4  Agent Studio (skipped: --no-studio) ===")
    else:
        info("=== 4/4  Agent Studio ===")
        setup_agent_studio.main()

    info("")
    info("All done. Start the servers with:  python setup/run_stack.py")
    info("Then launch the GUI:  cd agent_studio && flutter run -d linux")


if __name__ == "__main__":
    main()
