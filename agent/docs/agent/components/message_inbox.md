# MessageInbox

`include/agent/message_inbox.hpp` · `src/agent/message_inbox.cpp`

---

## Overview

`MessageInbox` is a thread-safe per-agent message queue. It implements **Pattern B** — direct agent-to-agent communication. Each agent has exactly one inbox, owned by its `AgentEntry` inside `AgentManager`.

Messages are pushed by senders (via `AgentManager::sendMessage` or `broadcast`) and drained by the receiving agent (via `AgentManager::drainInbox` or `ReceiveMessagesAction`).

---

## Message

```cpp
struct Message {
    std::string    from_id;    // sender agent id
    std::string    to_id;      // recipient agent id
    nlohmann::json payload;    // arbitrary JSON content
    std::string    timestamp;  // ISO-8601 UTC
};
```

`payload` carries the actual message content. There is no schema — any JSON value is valid.

---

## API

### `push`

```cpp
void push(Message msg);
```

Appends the message to the inbox queue. Thread-safe: may be called from any thread.

### `drain`

```cpp
std::vector<Message> drain();
```

Removes and returns all queued messages atomically. Returns an empty vector if the inbox is empty. Thread-safe: may be called from any thread.

### `empty`

```cpp
bool empty() const;
```

Returns `true` if there are no pending messages.

---

## Thread-Safety

All methods acquire `m_mutex` (a `std::mutex`). Concurrent pushes from multiple senders and a drain from the receiver thread are safe.

The underlying container is a `std::deque<Message>` — `drain` moves all elements out in O(N).

---

## Usage via AgentManager

```cpp
// Pattern B — send
manager.sendMessage("agent_1", "agent_2", {{"type", "ping"}, {"data", 42}});

// Pattern B — broadcast to all except sender
manager.broadcast("agent_1", {{"type", "update"}, {"value", "$result"}});

// Pattern B — receive
auto messages = manager.drainInbox("agent_2");
for (auto& m : messages) {
    // process m.payload
}
```

### Via Actions (inside an agent plan)

```json
{"name": "SendMessageAction",    "id": "send1", "inputs": {"to_id": "agent_2", "payload": {"x": 42}}}
{"name": "ReceiveMessagesAction","id": "recv1", "inputs": {}}
```

See [`actions.md`](actions.md) for full input/output schemas.

---

## Comparison with Blackboard (Pattern C)

| | `MessageInbox` | `Blackboard` |
|---|---|---|
| Addressing | Point-to-point or broadcast | Key-based lookup |
| Delivery | Once (drain removes) | Persistent until deleted |
| Ordering | FIFO queue | Last-write-wins |
| Best for | Coordination, handoff, notifications | Shared results, fan-in accumulation |

---

## Related Components

- [`AgentManager`](agent_manager.md) — owns one inbox per agent; exposes `sendMessage`, `broadcast`, `drainInbox`
- [`AgentContext`](agent.md) — exposes `inbox()` pointer to actions
- [`Actions`](actions.md) — `SendMessageAction`, `ReceiveMessagesAction`
- [`Blackboard`](blackboard.md) — shared-state alternative (Pattern C)
- [`EventBus`](event_bus.md) — emits `message_sent`/`message_received` events
