# 05 — Decommission (Dart servers)

**Action:** Delete. These duplicate logic the C++ engine/server own. Removal
happens in **Phase 3**, only after the C++ server (doc 02) is proven and the
Flutter web app points at it (Phases 1–2). Listed here so nothing is missed.

## Targets

### `agent_server/` (Dart)  — DELETE
Duplicate web API with its **own** in-memory store and its **own** LLM path
(`lib/llm_router.dart`, `lib/agent_loop.dart`) that never invokes the engine.
- **DEC-1 [MUST]** Replaced by `agent_server_cpp/` (doc 02).
- **DEC-2 [MUST]** Remove `agent_server/` entirely (bin, lib, pubspec, Dockerfile).

### `mcp_server/` (Dart)  — DELETE
Duplicate of the C++ `mcp-server/` (doc 04).
- **DEC-3 [MUST]** Remove `mcp_server/` entirely.

## Cross-references to update (don't leave dangling)
- **DEC-4 [MUST]** `docker-compose.yml`: `api` service builds `agent_server_cpp/`,
  not Dart; remove any Dart mcp service.
- **DEC-5 [MUST]** `Dockerfile.web`, `nginx.conf`: point the web app at the C++
  server's host/port.
- **DEC-6 [MUST]** `setup/`: update/remove `setup_*` scripts that install the Dart
  servers; `run_stack.py` / `setup_all.py` reference the C++ server.
- **DEC-7 [MUST]** `start.sh`, `deploy.sh`: drop `dart pub get` / `dart run` for
  the deleted servers; build/launch the C++ server.
- **DEC-8 [SHOULD]** Root docs / READMEs that mention the Dart servers.
- **DEC-9 [SHOULD]** Session-start hook does `dart pub get (agent_server)` and
  `dart pub get (mcp_server)` — remove those steps after deletion.

## Preconditions (must all be true before deleting)
- **DEC-10 [MUST]** C++ server passes doc 02 acceptance criteria.
- **DEC-11 [MUST]** Flutter web runs fully against the C++ server (chat, run,
  pipe, events, benchmark) — doc 03 acceptance criteria.
- **DEC-12 [SHOULD]** Tag/branch the repo before deletion for easy rollback.

## Acceptance criteria
- [ ] `agent_server/` and `mcp_server/` no longer exist.
- [ ] `grep -ri "agent_server\b\|mcp_server\b"` finds no live references in build/
      deploy/setup scripts (only historical docs).
- [ ] `docker compose up` runs the full stack with only C++ services + Flutter web.

## Change log
- _draft_ — initial requirements.
