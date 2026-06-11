#!/bin/bash
set -euo pipefail

# Only run in Claude Code remote (web) environments.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

FLUTTER_HOME="$HOME/flutter"
HTTPLIB_DIR="$CLAUDE_PROJECT_DIR/agent/third_party/httplib"
BUILD_DIR="$CLAUDE_PROJECT_DIR/agent/build"

# ── 1. Flutter SDK ────────────────────────────────────────────────────────────
if ! command -v flutter &>/dev/null; then
  if [ ! -d "$FLUTTER_HOME" ]; then
    echo "[session-start] Cloning Flutter stable SDK..."
    git clone https://github.com/flutter/flutter.git -b stable --depth 1 "$FLUTTER_HOME"
  fi
  export PATH="$FLUTTER_HOME/bin:$PATH"
  echo "export PATH=\"$FLUTTER_HOME/bin:\$PATH\"" >> "$CLAUDE_ENV_FILE"
fi

# Precache web and linux artifacts
flutter precache --web --linux \
  --no-android --no-ios --no-macos --no-windows --no-fuchsia \
  2>/dev/null || true

flutter config --no-analytics 2>/dev/null || true

# ── 2. Flutter app dependencies ───────────────────────────────────────────────
echo "[session-start] flutter pub get (agent_studio)..."
cd "$CLAUDE_PROJECT_DIR/agent_studio"
flutter pub get

# ── 3. Dart server dependencies ───────────────────────────────────────────────
echo "[session-start] dart pub get (agent_server)..."
cd "$CLAUDE_PROJECT_DIR/agent_server"
dart pub get

echo "[session-start] dart pub get (mcp_server)..."
cd "$CLAUDE_PROJECT_DIR/mcp_server"
dart pub get

# ── 4. cpp-httplib header ─────────────────────────────────────────────────────
if [ ! -f "$HTTPLIB_DIR/httplib.h" ]; then
  echo "[session-start] Fetching cpp-httplib header..."
  mkdir -p "$HTTPLIB_DIR"
  curl -fsSL \
    "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h" \
    -o "$HTTPLIB_DIR/httplib.h"
fi

# ── 5. C++ agent engine build ─────────────────────────────────────────────────
if [ ! -f "$BUILD_DIR/libagent_engine.so" ]; then
  echo "[session-start] Configuring C++ engine..."
  mkdir -p "$BUILD_DIR"
  cmake -S "$CLAUDE_PROJECT_DIR/agent" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DAGENT_ENABLE_API_LLM=ON \
    -DAGENT_ENABLE_MCP_HTTP=ON \
    -DHTTPLIB_INCLUDE_DIR="$HTTPLIB_DIR" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

  echo "[session-start] Building C++ engine..."
  cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

echo "[session-start] Setup complete."
