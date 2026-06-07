# Agent Studio

Interactive Flutter GUI for your AI agent engine. Create and manage single agents, agent hierarchies, and collaborative groups — all from a sleek dark-themed desktop/web UI.

## Features

| Feature | Description |
|---|---|
| **Agent Builder** | 4-step wizard — identity, model selection, tools, hierarchy placement |
| **Hierarchy Tree** | Drag-and-drop parent/child agent relationships with visual connectors |
| **Agent Groups** | 5 collaboration modes: Parallel, Sequential, Broadcast, Consensus, Pipeline |
| **Chat Panel** | Real-time conversation with any agent or group; markdown + code rendering |
| **Task Dispatch** | Assign tasks to individual agents, groups, or the full hierarchy |
| **Live Status** | Animated status badges — idle/running/waiting/done/error |
| **Task History** | Full task log with durations and results |
| **Event Log** | Internal event stream for debugging |
| **FFI Backend** | Direct `libagent_engine.so` loading via Dart FFI (no server needed) |
| **HTTP Fallback** | Connect to REST API or run in mock mode |

## Quick Start

```bash
# Install Flutter: https://docs.flutter.dev/get-started/install
flutter pub get
flutter run -d chrome        # web
flutter run -d linux         # Linux desktop
flutter run -d macos         # macOS desktop
flutter run -d windows       # Windows desktop
```

## Connecting to the Local Agent Engine

Open **Settings** (gear icon) and choose your connection mode:

### FFI (desktop only — recommended)
Point directly to your compiled shared library:
```
/path/to/libagent_engine.so
```
The app loads it directly via Dart FFI — no HTTP server needed. Build it with:
```bash
cmake -DBUILD_SHARED_LIBS=ON ..
make agent_engine
```

### HTTP REST
Connect to a running `AgentManager` server:
```
http://localhost:8080
```
Expected endpoints:
```
GET  /health
POST /api/agents          → spawn agent
GET  /api/agents/:id/status
POST /api/agents/:id/run  → { "task": "..." }
POST /api/agents/:id/cancel
POST /api/groups/:id/run
```

Without a connection the app runs in **mock mode** with simulated responses.

## Architecture

```
lib/
├── main.dart
├── theme/app_theme.dart
├── models/
│   ├── agent_model.dart
│   ├── agent_group.dart
│   └── task_model.dart
├── providers/agent_provider.dart
├── services/
│   ├── agent_api_service.dart
│   ├── engine_service_ffi.dart   (desktop)
│   └── engine_service_stub.dart  (web)
├── ffi/
│   ├── agent_engine_bindings.dart
│   └── agent_engine_ffi.dart
├── screens/
│   ├── dashboard_screen.dart
│   ├── agent_builder_dialog.dart
│   ├── group_builder_dialog.dart
│   └── settings_panel.dart
└── widgets/
    ├── agent_card.dart
    ├── chat_panel.dart
    ├── hierarchy_tree.dart
    └── status_badge.dart
```

## Group Modes

- **Parallel** — all agents receive the same task simultaneously
- **Sequential** — agent A → agent B → agent C (output chains)
- **Broadcast** — send to all, pick best result
- **Consensus** — all agents must agree before output is accepted
- **Pipeline** — waterfall transformation (stage A → B → C)
