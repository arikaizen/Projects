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
import site
import subprocess
import sys
import sysconfig
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
    """Make sure a C++ compiler is available (cmake needs one)."""
    for cc in ("g++", "clang++", "c++", "cl"):
        if shutil.which(cc):
            return
    if os.name == "nt":
        # MSVC's cl.exe is normally only on PATH inside a Developer prompt, but
        # CMake's Visual Studio generator finds it on its own. Don't hard-fail —
        # warn and let CMake's configure step surface a precise error if VS is
        # genuinely missing.
        warn(
            "no C++ compiler found on PATH. If CMake configure fails below, "
            "install one of:\n"
            "    - Visual Studio Build Tools (C++ workload): "
            "https://visualstudio.microsoft.com/downloads/\n"
            "    - or run this from a 'Developer Command Prompt for VS'."
        )
        return
    fail(
        "no C++ compiler found (looked for g++, clang++, c++). "
        "Install build tools, e.g.\n"
        "    Ubuntu/Debian:  sudo apt-get install build-essential\n"
        "    Fedora:         sudo dnf install gcc-c++ make\n"
        "    macOS:          xcode-select --install"
    )


def _find_pip_cmake() -> str | None:
    """Locate a cmake executable installed by `pip install cmake`.

    The pip package puts the real binary in <site-packages>/cmake/data/bin/ and
    a launcher in the interpreter's scripts dir — neither is guaranteed to be on
    PATH (a common Windows gotcha). We search both, plus the imported package's
    own location, and return the first hit.
    """
    exe = "cmake.exe" if os.name == "nt" else "cmake"
    candidate_dirs: list[str] = []

    # 1. The cmake package's bundled binary: <site-packages>/cmake/data/bin/.
    site_dirs: list[str] = []
    try:
        user_site = site.getusersitepackages()
        if isinstance(user_site, str):
            site_dirs.append(user_site)
    except Exception:
        pass
    try:
        site_dirs.extend(site.getsitepackages())
    except Exception:
        pass
    for sp in site_dirs:
        candidate_dirs.append(os.path.join(sp, "cmake", "data", "bin"))

    # 2. The imported package's location (most reliable after a fresh install).
    try:
        import importlib
        import cmake as _cmake_pkg  # type: ignore
        importlib.reload(_cmake_pkg)
        pkg_dir = os.path.dirname(_cmake_pkg.__file__)
        candidate_dirs.append(os.path.join(pkg_dir, "data", "bin"))
    except Exception:
        pass

    # 3. Every scripts dir Python knows about (where the launcher lands).
    for scheme in sysconfig.get_scheme_names():
        try:
            sdir = sysconfig.get_path("scripts", scheme)
            if sdir:
                candidate_dirs.append(sdir)
        except Exception:
            pass

    for d in candidate_dirs:
        path = os.path.join(d, exe)
        if os.path.isfile(path):
            # Make sibling tools (ctest/cpack) reachable for this process too.
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")
            return path
    return None


def ensure_cmake() -> str:
    """Return a path to a working cmake, installing it via pip if necessary."""
    cmake = shutil.which("cmake")
    if cmake:
        info(f"found cmake: {cmake}")
        return cmake

    info("cmake not found — installing it from PyPI (no sudo needed)...")
    run([sys.executable, "-m", "pip", "install", "--user", "cmake"])

    cmake = shutil.which("cmake") or _find_pip_cmake()
    if cmake:
        info(f"using pip-installed cmake: {cmake}")
        return cmake

    fail(
        "installed cmake via pip but can't locate the binary.\n"
        "    On Windows, add this directory to your PATH and re-run:\n"
        "        %APPDATA%\\Python\\Python3XX\\Scripts\n"
        "    (XX = your Python version, e.g. Python314). Then restart the shell.\n"
        "    Or install a system cmake from https://cmake.org/download/ and re-run."
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
