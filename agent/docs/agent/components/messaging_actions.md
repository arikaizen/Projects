# Messaging Actions

`src/agent/actions/messaging_actions.hpp` · `src/agent/actions/messaging_actions.cpp`
**Kind:** Action · **Pattern:** B (agent-to-agent messaging)
Registered by `registerMessagingActions(factory)`.

This file provides two actions for **Pattern B** messaging via each agent's [`MessageInbox`](message_inbox.md).

---

## SendMessageAction

**Factory name:** `SendMessageAction`

Sends a JSON message to another agent's inbox via `AgentManager::sendMessage`.

### Inputs

| Field | Type | Required | Description |
|---|---|---|---|
| `to` | string | **yes** | Destination agent id |
| `message` | any JSON | **yes** | Arbitrary payload to send |

### Output

```json
{"sent": true, "to": "agent_2"}
```

---

## ReceiveMessagesAction

**Factory name:** `ReceiveMessagesAction`

Drains this agent's inbox and returns all pending messages.

### Inputs

None.

### Output

```json
{"messages": [{"from_id": "agent_1", "to_id": "agent_2", "payload": {...}, "timestamp": "..."}]}
```

---

## Thread-Safety

`AgentManager::sendMessage` and `drainInbox`, and `MessageInbox` itself, are mutex-guarded — safe to call concurrently.

## Related

- [Actions overview](actions.md) · [MessageInbox](message_inbox.md) — the underlying queue
- [AgentManager](agent_manager.md) — `sendMessage`/`broadcast`/`drainInbox`
- [Blackboard actions](blackboard_actions.md) — Pattern C alternative
