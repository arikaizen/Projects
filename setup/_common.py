"""Shared helpers for the Projects setup scripts.

These scripts let you build the whole stack **without a system cmake** — if
cmake is missing they install it from PyPI (`pip install cmake`), which ships a
self-contained cmake binary on Linux/macOS/Windows and needs no sudo.

Nothing here is project-specific beyond paths; see the individual
``setup_*.py`` scripts for what each component needs.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

# ── Paths ────────────────────────────────────────────────────────────────────
# setup/ lives at the repo root, so the root is this file's grandparent's parent.
REPO_ROOT = Path(__file__).resolve().parent.parent
HTTPLIB_DIR = REPO_ROOT / "agent" / "third_party" / "httplib"

AUTH_SERVER_DIR = REPO_ROOT / "auth-server"
MCP_SERVER_DIR = REPO_ROOT / "mcp-server"
AGENT_DIR = REPO_ROOT / "agent"
AGENT_STUDIO_DIR = REPO_ROOT / "agent_studio"
DATA_SERVER_DIR = REPO_ROOT / "data server"


# ── Pretty printing ────────────────────────────────────────────────────────--
def info(msg: str) -> None:
    print(f"\033[1;34m[setup]\033[0m {msg}", flush=True)


def warn(msg: str) -> None:
    print(f"\033[1;33m[setup] WARNING:\033[0m {msg}", flush=True)


def fail(msg: str) -> "NoReturn":  # type: ignore[name-defined]
    print(f"\033[1;31m[setup] ERROR:\033[0m {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


# ── Command runner ─────────────────────────────────────────────────────────--
def run(cmd: list[str], cwd: Path | None = None, env: dict | None = None) -> None:
    """Run a command, echoing it first; abort the script on non-zero exit."""
    printable = " ".join(str(c) for c in cmd)
    where = f" (in {cwd})" if cwd else ""
    info(f"$ {printable}{where}")
    result = subprocess.run([str(c) for c in cmd], cwd=str(cwd) if cwd else None, env=env)
    if result.returncode != 0:
        fail(f"command failed (exit {result.returncode}): {printable}")


# ── Toolchain bootstrap ────────────────────────────────────────────────────--
def ensure_compiler() -> None:
    """Make sure a C++ compiler is on PATH (cmake needs one)."""
    for cc in ("g++", "clang++", "c++"):
        if shutil.which(cc):
            return
    fail(
        "no C++ compiler found (looked for g++, clang++, c++). "
        "Install build tools, e.g.\n"
        "    Ubuntu/Debian:  sudo apt-get install build-essential\n"
        "    Fedora:         sudo dnf install gcc-c++ make\n"
        "    macOS:          xcode-select --install"
    )


def ensure_cmake() -> str:
    """Return a path to a working cmake, installing it via pip if necessary."""
    cmake = shutil.which("cmake")
    if cmake:
        info(f"found cmake: {cmake}")
        return cmake

    info("cmake not found — installing it from PyPI (no sudo needed)...")
    run([sys.executable, "-m", "pip", "install", "--user", "cmake"])

    # pip may install into a user scripts dir that isn't on PATH yet; find it.
    cmake = shutil.which("cmake")
    if cmake:
        return cmake
    try:
        import cmake as _cmake_pkg  # type: ignore

        candidate = Path(_cmake_pkg.CMAKE_BIN_DIR) / "cmake"
        if candidate.exists():
            info(f"using pip-installed cmake: {candidate}")
            return str(candidate)
    except Exception:
        pass
    fail(
        "installed cmake via pip but can't locate the binary. "
        "Add Python's user-scripts dir to PATH and re-run, or "
        "`python -m pip install --user cmake` then restart your shell."
    )


def ensure_httplib() -> None:
    if not (HTTPLIB_DIR / "httplib.h").exists():
        fail(
            f"cpp-httplib header not found at {HTTPLIB_DIR / 'httplib.h'}. "
            "It is vendored in the repo — make sure you pulled the latest branch."
        )


def cpu_count() -> int:
    return os.cpu_count() or 4


# ── Generic cmake build ────────────────────────────────────────────────────--
def cmake_build(
    src_dir: Path,
    *,
    defines: dict[str, str] | None = None,
    target: str | None = None,
    build_type: str = "Release",
) -> Path:
    """Configure + build a cmake project. Returns the build directory."""
    ensure_compiler()
    ensure_httplib()
    cmake = ensure_cmake()
    build_dir = src_dir / "build"

    config_cmd = [
        cmake,
        "-S", src_dir,
        "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DHTTPLIB_INCLUDE_DIR={HTTPLIB_DIR}",
    ]
    for key, value in (defines or {}).items():
        config_cmd.append(f"-D{key}={value}")
    run(config_cmd)

    build_cmd = [cmake, "--build", build_dir, "-j", str(cpu_count())]
    if target:
        build_cmd += ["--target", target]
    run(build_cmd)
    return build_dir
