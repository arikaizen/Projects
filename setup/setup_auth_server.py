#!/usr/bin/env python3
"""Build the OAuth 2.1 auth server (C++).

Produces: auth-server/build/auth_server  (listens on :8080)

Needs: a C++ compiler, OpenSSL dev headers, cmake (auto-installed via pip if
missing). nlohmann/json is fetched automatically by the project's CMakeLists.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import AUTH_SERVER_DIR, cmake_build, fail, info  # noqa: E402


def main() -> None:
    if not AUTH_SERVER_DIR.exists():
        fail(f"auth-server directory not found at {AUTH_SERVER_DIR}")

    info("Building auth-server...")
    build_dir = cmake_build(AUTH_SERVER_DIR, target="auth_server")

    binary = build_dir / "auth_server"
    if not binary.exists():
        fail(f"build finished but {binary} was not produced")
    info(f"OK — auth server binary at {binary}")
    info("Run it with:  python setup/run_stack.py   (or ./auth-server/build/auth_server)")


if __name__ == "__main__":
    main()
