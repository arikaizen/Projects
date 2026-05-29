// blackboard.cpp — Blackboard implementation
#include "agent/blackboard.hpp"
#include "agent/event_bus.hpp"

#include <iostream>

namespace agent {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Blackboard::Blackboard(EventBus* event_bus)
    : m_event_bus(event_bus)
{}

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------
void Blackboard::write(const std::string& key, nlohmann::json value)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_data[key] = std::move(value);
    }

    if (m_event_bus) {
        m_event_bus->emit(
            EventBus::makeEvent("blackboard_updated",
                                {{"key", key}}));
    }
}

// ---------------------------------------------------------------------------
// read
// ---------------------------------------------------------------------------
std::optional<nlohmann::json> Blackboard::read(const std::string& key) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_data.find(key);
    if (it == m_data.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------------
// remove
// ---------------------------------------------------------------------------
void Blackboard::remove(const std::string& key)
{
    bool erased = false;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        erased = (m_data.erase(key) > 0);
    }

    if (erased && m_event_bus) {
        m_event_bus->emit(
            EventBus::makeEvent("blackboard_removed",
                                {{"key", key}}));
    }
}

// ---------------------------------------------------------------------------
// keys — return all keys with the given prefix (empty prefix = all keys)
// ---------------------------------------------------------------------------
std::vector<std::string> Blackboard::keys(const std::string& prefix) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [k, _] : m_data) {
        if (prefix.empty() || k.rfind(prefix, 0) == 0) {
            result.push_back(k);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// contains
// ---------------------------------------------------------------------------
bool Blackboard::contains(const std::string& key) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_data.count(key) > 0;
}

} // namespace agent
