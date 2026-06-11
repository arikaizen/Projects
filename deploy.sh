#!/usr/bin/env bash
# deploy.sh — Build Agent Studio for remote access and start all services
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
WEB_DIR="$PROJECT_DIR/agent_studio/build/web"
SERVER_DIR="$PROJECT_DIR/agent_server"

HOST_IP="${HOST_IP:-0.0.0.0}"
WEB_PORT="${WEB_PORT:-8080}"
API_PORT="${API_PORT:-3001}"

echo "=== Agent Studio Production Build ==="
echo "Web port : $WEB_PORT"
echo "API port : $API_PORT"
echo ""

# ── 1. Build Flutter web app ──────────────────────────────────────────────────
echo "[1/3] Building Flutter web app..."
cd "$PROJECT_DIR/agent_studio"
flutter build web --release --no-tree-shake-icons
echo "      Done — output: $WEB_DIR"

# ── 2. Compile agent_server ───────────────────────────────────────────────────
echo "[2/3] Compiling agent_server..."
cd "$SERVER_DIR"
dart compile exe bin/server.dart -o bin/agent_server
echo "      Done — binary: $SERVER_DIR/bin/agent_server"

# ── 3. Start services ─────────────────────────────────────────────────────────
echo "[3/3] Starting services..."

# Serve the Flutter web build with a simple Dart HTTP server
# (or use nginx — see README)
if ! command -v python3 &>/dev/null; then
  echo "ERROR: python3 not found. Install it or use nginx to serve $WEB_DIR"
  exit 1
fi

# Start agent_server in background
cd "$SERVER_DIR"
nohup ./bin/agent_server --host "$HOST_IP" --port "$API_PORT" > /tmp/agent_server.log 2>&1 &
API_PID=$!
echo "      agent_server PID $API_PID  →  http://0.0.0.0:$API_PORT"
echo "$API_PID" > /tmp/agent_server.pid

# Serve web build
echo "      Flutter web     →  http://0.0.0.0:$WEB_PORT"
echo ""
echo "Open  http://YOUR_SERVER_IP:$WEB_PORT  on the remote computer."
echo "Then enter  http://YOUR_SERVER_IP:$API_PORT  as the Cloud Server URL."
echo ""
echo "Press Ctrl+C to stop the web server (agent_server will keep running)."
echo "To stop agent_server:  kill \$(cat /tmp/agent_server.pid)"
echo ""
cd "$WEB_DIR"
python3 -m http.server "$WEB_PORT"
