#pragma once
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

struct Message {
    std::string    from_id;
    std::string    to_id;
    nlohmann::json payload;
    std::string    timestamp;  // ISO-8601 UTC
};

// Thread-safe per-agent message inbox (Pattern B).
class MessageInbox {
public:
    void push(Message msg);
    std::vector<Message> drain();  // removes and returns all queued messages
    bool empty() const;

private:
    mutable std::mutex     m_mutex;
    std::deque<Message>    m_messages;
};

} // namespace agent
