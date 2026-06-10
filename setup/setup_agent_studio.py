#!/usr/bin/env python3
"""Prepare the Flutter GUI (Agent Studio).

Runs `flutter pub get`. This needs the Flutter SDK, which cannot be pip-
installed — if Flutter is missing the script prints install instructions and
exits without failing the rest of the setup.

Run the app yourself afterwards:
    cd agent_studio && flutter run -d linux     # desktop (uses the engine via FFI)
    cd agent_studio && flutter run -d chrome     # web (calls provider/MCP APIs directly)
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from _common import AGENT_STUDIO_DIR, fail, info, run, warn  # noqa: E402


def main() -> None:
    if not AGENT_STUDIO_DIR.exists():
        fail(f"agent_studio directory not found at {AGENT_STUDIO_DIR}")

    if not shutil.which("flutter"):
        warn(
            "Flutter SDK not found — skipping `flutter pub get`.\n"
            "    Install it from https://docs.flutter.dev/get-started/install\n"
            "    then re-run:  cd agent_studio && flutter pub get"
        )
        return

    info("Fetching Flutter packages...")
    run([shutil.which("flutter"), "pub", "get"], cwd=AGENT_STUDIO_DIR)
    info("OK — run the app with:  cd agent_studio && flutter run -d linux   (or -d chrome)")


if __name__ == "__main__":
    main()
