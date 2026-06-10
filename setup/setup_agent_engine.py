#!/usr/bin/env python3
"""Build the C++ agent engine as a shared library.

Produces: agent/build/libagent_engine.so  (loaded by Agent Studio via FFI;
not a server — it has no port)

Built with the hosted-LLM backends and live MCP HTTP transport enabled:
  -DAGENT_ENABLE_API_LLM=ON   OpenAI/Claude/Gemini/Ollama + OpenAI-compatible
  -DAGENT_ENABLE_MCP_HTTP=ON  real JSON-RPC tool calls to the MCP server

OpenSSL is auto-detected by CMake; with it, https providers work, otherwise
only http:// local servers (Ollama, LM Studio, ...) are reachable.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import AGENT_DIR, cmake_build, fail, info  # noqa: E402


def main() -> None:
    if not AGENT_DIR.exists():
        fail(f"agent directory not found at {AGENT_DIR}")

    info("Building agent engine (libagent_engine.so) with API LLM + MCP HTTP...")
    build_dir = cmake_build(
        AGENT_DIR,
        defines={
            "AGENT_ENABLE_API_LLM": "ON",
            "AGENT_ENABLE_MCP_HTTP": "ON",
        },
        target="agent_engine",
    )

    # CMake names it libagent_engine.so / .dylib / agent_engine.dll by platform.
    artifacts = (
        list(build_dir.glob("libagent_engine.*"))
        + list(build_dir.glob("agent_engine.*"))
    )
    artifacts = [p for p in artifacts if p.suffix in (".so", ".dylib", ".dll")]
    if not artifacts:
        fail(f"build finished but no agent_engine library found under {build_dir}")
    info(f"OK — engine library at {artifacts[0]}")
    info("In Agent Studio: Settings -> Connection -> FFI, point it at this path.")


if __name__ == "__main__":
    main()
