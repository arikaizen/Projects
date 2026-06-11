# Setup scripts

Python scripts that build and run the whole stack **without needing cmake
preinstalled** — if `cmake` is missing they install it from PyPI
(`pip install cmake`), which ships a self-contained binary and needs no sudo.

## What you need first

- **Python 3** (you're using it to run these scripts)
- **A C++ compiler** — `g++`, `clang++`, or `c++` on PATH
  - Ubuntu/Debian: `sudo apt-get install build-essential`
  - Fedora: `sudo dnf install gcc-c++ make`
  - macOS: `xcode-select --install`
- **OpenSSL dev headers** (for the auth + MCP servers' JWT/TLS)
  - Ubuntu/Debian: `sudo apt-get install libssl-dev`
  - Fedora: `sudo dnf install openssl-devel`
  - macOS: `brew install openssl`
- **Flutter SDK** (only for the GUI) — https://docs.flutter.dev/get-started/install

cmake, cpp-httplib, and nlohmann/json are handled automatically.

## Build everything

```bash
python setup/setup_all.py            # builds all 4 components
python setup/setup_all.py --no-studio  # skip the Flutter step
```

Or build one component at a time:

```bash
python setup/setup_auth_server.py    # -> auth-server/build/auth_server   (:8080)
python setup/setup_mcp_server.py     # -> mcp-server/build/mcp_server      (:8081)
python setup/setup_agent_engine.py   # -> agent/build/libagent_engine.so   (FFI lib)
python setup/setup_agent_studio.py   # -> flutter pub get
```

## Run the servers

```bash
python setup/run_stack.py            # starts auth (:8080) then MCP (:8081)
```

It waits for the auth server's `/health` before starting the MCP server (which
depends on it for token validation), loads `mcp-server/.env` for API keys, and
stops both cleanly on Ctrl+C.

## Run the GUI

The agent engine is a **library**, not a server — Agent Studio loads it via FFI.

```bash
cd agent_studio
flutter run -d linux     # desktop: Settings -> Connection -> FFI -> agent/build/libagent_engine.so
flutter run -d chrome    # web: calls provider/MCP APIs directly from the browser
```

## Start order (what depends on what)

| # | Component | How it starts | Port | Depends on |
|---|-----------|---------------|------|------------|
| 1 | Auth server | `run_stack.py` | 8080 | — |
| 2 | MCP server | `run_stack.py` | 8081 | auth server :8080 |
| 3 | Agent engine | loaded by the GUI (FFI) | — | MCP :8081 at tool-call time |
| 4 | Agent Studio | `flutter run` | — | engine `.so`, an LLM provider, MCP |

API keys for the MCP tools go in `mcp-server/.env` (seeded from `.env.example`
by `setup_mcp_server.py`). They stay server-side and are never sent to the model.
