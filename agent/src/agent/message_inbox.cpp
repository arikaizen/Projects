// message_inbox.cpp — MessageInbox implementation
#include "agent/message_inbox.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace agent {

// ---------------------------------------------------------------------------
// push
// ---------------------------------------------------------------------------
void MessageInbox::push(Message msg)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_messages.push_back(std::move(msg));
}

// ---------------------------------------------------------------------------
// drain — move all messages out and return them
// ---------------------------------------------------------------------------
std::vector<Message> MessageInbox::drain()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::vector<Message> result(
        std::make_move_iterator(m_messages.begin()),
        std::make_move_iterator(m_messages.end()));
    m_messages.clear();
    return result;
}

// ---------------------------------------------------------------------------
// empty
// ---------------------------------------------------------------------------
bool MessageInbox::empty() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_messages.empty();
}

} // namespace agent
