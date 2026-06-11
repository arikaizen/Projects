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

free_port() {
  # Kill any process currently bound to the given TCP port.
  local port="$1"
  if command -v fuser &>/dev/null; then
    fuser -k "${port}/tcp" 2>/dev/null || true
  elif command -v lsof &>/dev/null; then
    local pids; pids="$(lsof -ti tcp:"$port" 2>/dev/null || true)"
    [ -n "$pids" ] && kill $pids 2>/dev/null || true
  fi
}

start_svc() {
  local name="$1"; shift
  local pidfile="$RUN_DIR/$name.pid"
  # Kill any previous instance tracked by this script
  [ -f "$pidfile" ] && kill "$(cat "$pidfile")" 2>/dev/null || true
  nohup "$@" > "$RUN_DIR/$name.log" 2>&1 &
  echo "$!" > "$pidfile"
  echo "  $name started (PID $!) — log: $RUN_DIR/$name.log"
}

# Free ports in case stale processes (or a manual run) still hold them
free_port "$API_PORT"
free_port "$MCP_PORT"
free_port "$WEB_PORT"

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

# Locate cloudflared, installing it if necessary. Echoes the binary path.
ensure_cloudflared() {
  # Already on PATH?
  if command -v cloudflared &>/dev/null; then
    command -v cloudflared
    return 0
  fi
  # Previously installed by this script?
  if [ -x "$RUN_DIR/cloudflared" ]; then
    echo "$RUN_DIR/cloudflared"
    return 0
  fi
  # A copy lying around the repo from an earlier manual download?
  if [ -x "$PROJECT_DIR/agent_studio/cloudflared" ]; then
    echo "$PROJECT_DIR/agent_studio/cloudflared"
    return 0
  fi
  # Download it (no sudo needed — kept inside .run/)
  echo "  cloudflared not found — downloading..." >&2
  local arch bin
  case "$(uname -m)" in
    x86_64|amd64) arch=amd64 ;;
    aarch64|arm64) arch=arm64 ;;
    *) arch=amd64 ;;
  esac
  bin="$RUN_DIR/cloudflared"
  if curl -fsSL \
      "https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-${arch}" \
      -o "$bin" 2>/dev/null; then
    chmod +x "$bin"
    echo "$bin"
    return 0
  fi
  return 1
}

if [ "$DO_TUNNEL" = "1" ]; then
  echo ""
  echo "=== [4/4] Opening public Cloudflare tunnel ==="
  CF_BIN="$(ensure_cloudflared || true)"
  if [ -z "${CF_BIN:-}" ] || [ ! -x "$CF_BIN" ]; then
    echo "  Could not install cloudflared automatically (no internet?)."
    echo "  The app still works on your LAN at http://${LAN_IP:-localhost}:$WEB_PORT"
  else
    free_port 20241  # cloudflared metrics port, in case it's stuck
    : > "$RUN_DIR/tunnel.log"   # clear old URL so we don't grab a stale one
    start_svc tunnel "$CF_BIN" tunnel --url "http://localhost:$WEB_PORT"
    echo "  Waiting for public URL (up to 60s)..."
    url=""
    for _ in $(seq 1 60); do
      url="$(grep -oE 'https://[a-z0-9-]+\.trycloudflare\.com' "$RUN_DIR/tunnel.log" 2>/dev/null | head -1)"
      [ -n "$url" ] && break
      # Bail early if the tunnel process died
      if [ -f "$RUN_DIR/tunnel.pid" ] && ! kill -0 "$(cat "$RUN_DIR/tunnel.pid")" 2>/dev/null; then
        break
      fi
      sleep 1
    done
    if [ -n "$url" ]; then
      # Persist the URL so it's easy to find again
      echo "$url" > "$RUN_DIR/public_url.txt"
      echo ""
      echo "  ============================================================"
      echo "    PUBLIC URL (open this from work):"
      echo ""
      echo "      $url"
      echo ""
      echo "  ============================================================"
    else
      echo "  Tunnel did not report a URL. Last log lines:"
      tail -n 15 "$RUN_DIR/tunnel.log" 2>/dev/null | sed 's/^/    /'
      echo "  Re-check later with: grep trycloudflare $RUN_DIR/tunnel.log"
    fi
  fi
fi

echo ""
echo "Login: admin / admin123   (or user / user123)"
echo "Everything runs in the background. To stop it all:  ./start.sh --stop"
echo "Public URL saved in: $RUN_DIR/public_url.txt"
echo "Logs are in: $RUN_DIR/"
