# MessageInbox

`include/agent/message_inbox.hpp` · `src/agent/message_inbox.cpp`

## Overview

`MessageInbox` is a thread-safe, per-agent MPSC (multiple-producer, single-consumer) message queue. It implements the receive side of **Pattern B** (agent-to-agent messaging). Each agent registered with `AgentManager` gets its own `MessageInbox`.

## Message Structure

```cpp
struct Message {
    std::string    from_id;
    std::string    to_id;
    nlohmann::json payload;
    std::string    timestamp;   // ISO-8601 UTC
};
```

| Field | Meaning |
|---|---|
| `from_id` | Sender agent ID |
| `to_id` | Recipient agent ID |
| `payload` | Arbitrary JSON message body |
| `timestamp` | UTC timestamp set by `AgentManager::sendMessage` |

## Interface

```cpp
void push(Message msg);
std::vector<Message> drain();
bool empty() const;
```

### `push`

Appends `msg` to the back of the internal `std::deque`. Called by `AgentManager::sendMessage` and `AgentManager::broadcast`. Thread-safe via `m_mutex`.

### `drain`

Atomically removes and returns all queued messages. After `drain()`, the inbox is empty. Called by `AgentManager::drainInbox` and wrapped by `ReceiveMessagesAction`. Thread-safe via `m_mutex`.

### `empty`

Returns `true` if no messages are queued. Thread-safe.

## Thread-Safety

All three methods acquire `m_mutex` for the duration of the operation. Multiple agents may call `push` concurrently; the single consumer calls `drain` from the agent loop.

## Usage Pattern

An agent that expects messages should include `ReceiveMessagesAction` in its plan periodically:

```json
{ "name": "ReceiveMessagesAction", "id": "recv1", "inputs": {} }
```

The result `recv1.messages` is then available as a `$ref` for downstream processing.

## Related Components

- [`SendMessageAction`](messaging_actions.md) — pushes messages into another agent's inbox
- [`ReceiveMessagesAction`](messaging_actions.md) — drains this agent's inbox
- [`AgentManager`](agent_manager.md) — owns one `MessageInbox` per agent; routes `sendMessage` and `broadcast`
- [`AgentContext`](agent_context.md) — `ctx.inbox()` non-owning pointer
