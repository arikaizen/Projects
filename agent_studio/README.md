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
| **Backend Connect** | Plug directly into your C++ `AgentManager` REST API or use mock mode |

## Quick Start

```bash
# Install Flutter: https://docs.flutter.dev/get-started/install
flutter pub get
flutter run -d chrome        # web
flutter run -d linux         # Linux desktop
flutter run -d macos         # macOS desktop
flutter run -d windows       # Windows desktop
```

## Connecting to the C++ Backend

In the app, open **Settings** (gear icon) and enter your backend URL:

```
http://localhost:8080
```

The app expects these REST endpoints from your `AgentManager`:

```
GET  /health                       → 200 OK
POST /api/agents                   → spawn agent
GET  /api/agents/:id/status        → get status
POST /api/agents/:id/run           → run task { "task": "..." }
POST /api/agents/:id/cancel        → cancel
POST /api/groups/:id/run           → run task on group
```

Without a connection the app runs in **mock mode** with simulated responses.

## Architecture

```
lib/
├── main.dart                  entry point
├── theme/
│   └── app_theme.dart         dark design system + colors
├── models/
│   ├── agent_model.dart       AgentModel, ChatMessage, AgentTool
│   ├── agent_group.dart       AgentGroup, GroupMode
│   └── task_model.dart        TaskModel
├── providers/
│   └── agent_provider.dart    central state (ChangeNotifier)
├── services/
│   └── agent_api_service.dart REST client + mock fallback
├── screens/
│   ├── dashboard_screen.dart  main layout + all tabs
│   ├── agent_builder_dialog.dart  4-step agent creation wizard
│   ├── group_builder_dialog.dart  group creation with mode picker
│   └── settings_panel.dart    backend connection settings
└── widgets/
    ├── agent_card.dart        agent card with status + actions
    ├── chat_panel.dart        full chat/task interface
    ├── hierarchy_tree.dart    drag-and-drop tree view
    └── status_badge.dart      animated status dot
```

## Group Modes

- **Parallel** — all agents receive the same task simultaneously
- **Sequential** — agent A → agent B → agent C (output chains)
- **Broadcast** — send to all, pick best result
- **Consensus** — all agents must agree before output is accepted
- **Pipeline** — waterfall transformation (stage A transforms → B transforms → C)
