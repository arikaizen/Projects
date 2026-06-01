# Messaging Actions

`src/agent/actions/messaging_actions.hpp` · `src/agent/actions/messaging_actions.cpp`

## Overview

The messaging actions implement **Pattern B** (agent-to-agent messaging). Messages are arbitrary JSON payloads delivered via `MessageInbox`. Two action types are provided: `SendMessageAction` and `ReceiveMessagesAction`.

---

## SendMessageAction

Sends a JSON message to another agent's inbox.

### Factory Registration

```
name:  "SendMessageAction"
kind:  Action
```

### Input Schema

| Input | Type | Required | Description |
|---|---|---|---|
| `to` | string | Yes | Destination agent ID |
| `message` | any | Yes | Arbitrary JSON payload |

### Execution

1. Resolves `$ref` values in inputs.
2. Calls `ctx.manager()->sendMessage(from_id, to, message)`.
3. `AgentManager` places a `Message{from_id, to, payload, timestamp}` in the destination's `MessageInbox`.

### Output

| Field | Value |
|---|---|
| `success` | `true` if the message was enqueued |
| `output.sent` | `true` |
| `output.to` | Destination agent ID |

---

## ReceiveMessagesAction

Drains this agent's inbox and returns all pending messages.

### Factory Registration

```
name:  "ReceiveMessagesAction"
kind:  Action
```

### Input Schema

No inputs required.

### Execution

1. Calls `ctx.manager()->drainInbox(this_agent_id)`.
2. `AgentManager` atomically removes and returns all messages from the agent's `MessageInbox`.

### Output

| Field | Value |
|---|---|
| `success` | `true` |
| `output.messages` | JSON array of message objects |

Each message:

```json
{
  "from_id": "agent-A",
  "to_id": "agent-B",
  "payload": { "key": "value" },
  "timestamp": "2025-01-01T00:00:00Z"
}
```

---

## Pattern B Flow

```
Agent A                        Agent B
  ↓                              ↓
SendMessageAction             ReceiveMessagesAction
  → AgentManager::sendMessage   ← AgentManager::drainInbox
        ↓                              ↑
    MessageInbox (B)  ───────────────
```

Agents polling their inbox should include `ReceiveMessagesAction` in their plans periodically to avoid message buildup.

## Related Components

- [`Action`](action.md) — base class
- [`MessageInbox`](message_inbox.md) — the per-agent queue these actions operate on
- [`AgentManager`](agent_manager.md) — `sendMessage` and `drainInbox`
