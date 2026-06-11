#!/usr/bin/env bash
# start.sh — Build and launch the entire Agent Studio stack on this machine.
#
#   ./start.sh            # build everything, start all services + public tunnel
#   ./start.sh --no-build # skip compilation, just start services
#   ./start.sh --no-tunnel# don't open a Cloudflare tunnel (LAN only)
#   ./start.sh --stop     # stop everything started by this script
#
# Services:
#   - Flutter web GUI       http://0.0.0.0:8080
#   - agent_server (cloud)  http://0.0.0.0:3001
#   - mcp_server            http://0.0.0.0:3000
#   - Cloudflare tunnel     public https URL -> web GUI
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
WEB_PORT="${WEB_PORT:-8080}"
API_PORT="${API_PORT:-3001}"
MCP_PORT="${MCP_PORT:-3000}"
RUN_DIR="$PROJECT_DIR/.run"
mkdir -p "$RUN_DIR"

DO_BUILD=1
DO_TUNNEL=1

for arg in "$@"; do
  case "$arg" in
    --no-build)  DO_BUILD=0 ;;
    --no-tunnel) DO_TUNNEL=0 ;;
    --stop)      DO_STOP=1 ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

# ── Stop mode ─────────────────────────────────────────────────────────────────
stop_all() {
  echo "Stopping all services..."
  for pidfile in "$RUN_DIR"/*.pid; do
    [ -f "$pidfile" ] || continue
    pid="$(cat "$pidfile")"
    if kill "$pid" 2>/dev/null; then
      echo "  stopped $(basename "$pidfile" .pid) (PID $pid)"
    fi
    rm -f "$pidfile"
  done
  echo "Done."
}

if [ "${DO_STOP:-0}" = "1" ]; then
  stop_all
  exit 0
fi

# Make sure flutter/dart are on PATH
export PATH="$HOME/flutter/bin:$PATH"

# ── 1. Build ──────────────────────────────────────────────────────────────────
if [ "$DO_BUILD" = "1" ]; then
  echo "=== [1/4] Building Flutter web GUI ==="
  cd "$PROJECT_DIR/agent_studio"
  flutter pub get
  flutter build web --release --no-tree-shake-icons

  echo "=== [2/4] Compiling agent_server ==="
  cd "$PROJECT_DIR/agent_server"
  dart pub get
  dart compile exe bin/server.dart -o bin/agent_server

  echo "=== Compiling mcp_server ==="
  cd "$PROJECT_DIR/mcp_server"
  dart pub get
  dart compile exe bin/server.dart -o bin/mcp_server
else
  echo "=== Skipping build (--no-build) ==="
fi

# ── 2. Start backend services ─────────────────────────────────────────────────
echo "=== [3/4] Starting services ==="

start_svc() {
  local name="$1"; shift
  local pidfile="$RUN_DIR/$name.pid"
  # Kill any previous instance
  [ -f "$pidfile" ] && kill "$(cat "$pidfile")" 2>/dev/null || true
  nohup "$@" > "$RUN_DIR/$name.log" 2>&1 &
  echo "$!" > "$pidfile"
  echo "  $name started (PID $!) — log: $RUN_DIR/$name.log"
}

# agent_server (cloud agents) — bind to all interfaces
start_svc agent_server \
  "$PROJECT_DIR/agent_server/bin/agent_server" --host 0.0.0.0 --port "$API_PORT"

# mcp_server — bind to all interfaces (override its 127.0.0.1 default)
start_svc mcp_server \
  "$PROJECT_DIR/mcp_server/bin/mcp_server" --host 0.0.0.0 --port "$MCP_PORT"

# Flutter web GUI via python http server
start_svc web \
  python3 -m http.server "$WEB_PORT" --bind 0.0.0.0 \
  --directory "$PROJECT_DIR/agent_studio/build/web"

sleep 2
echo ""
echo "Local URLs:"
LAN_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
echo "  Web GUI      http://${LAN_IP:-localhost}:$WEB_PORT"
echo "  agent_server http://${LAN_IP:-localhost}:$API_PORT"
echo "  mcp_server   http://${LAN_IP:-localhost}:$MCP_PORT"

# ── 3. Public tunnel ──────────────────────────────────────────────────────────
if [ "$DO_TUNNEL" = "1" ]; then
  echo ""
  echo "=== [4/4] Opening public Cloudflare tunnel ==="
  if ! command -v cloudflared &>/dev/null; then
    echo "  cloudflared not installed. Install with:"
    echo "    curl -L https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64 -o cloudflared"
    echo "    chmod +x cloudflared && sudo mv cloudflared /usr/local/bin/"
    echo "  Skipping tunnel. Services are still running on the LAN."
  else
    start_svc tunnel cloudflared tunnel --url "http://localhost:$WEB_PORT"
    echo "  Waiting for public URL..."
    for _ in $(seq 1 15); do
      url="$(grep -oE 'https://[a-z0-9-]+\.trycloudflare\.com' "$RUN_DIR/tunnel.log" 2>/dev/null | head -1)"
      [ -n "$url" ] && break
      sleep 1
    done
    if [ -n "${url:-}" ]; then
      echo ""
      echo "  ┌────────────────────────────────────────────────────────┐"
      echo "  │ PUBLIC URL (open this from work):                      │"
      echo "  │   $url"
      echo "  └────────────────────────────────────────────────────────┘"
    else
      echo "  Tunnel starting — check the URL with: tail -f $RUN_DIR/tunnel.log"
    fi
  fi
fi

echo ""
echo "Login: admin / admin123   (or user / user123)"
echo "Everything runs in the background. To stop it all:  ./start.sh --stop"
echo "Logs are in: $RUN_DIR/"
