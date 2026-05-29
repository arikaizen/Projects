// event_bus.cpp — EventBus implementation
#include "agent/event_bus.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace agent {

// ---------------------------------------------------------------------------
// Internal helper — ISO-8601 UTC timestamp
// ---------------------------------------------------------------------------
static std::string nowISO8601()
{
    auto tp   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ---------------------------------------------------------------------------
// subscribe
// ---------------------------------------------------------------------------
void EventBus::subscribe(EventCallback cb, void* key)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_subs.push_back(Sub{std::move(cb), key});
}

// ---------------------------------------------------------------------------
// unsubscribe
// ---------------------------------------------------------------------------
void EventBus::unsubscribe(void* key)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_subs.erase(
        std::remove_if(m_subs.begin(), m_subs.end(),
                       [key](const Sub& s) { return s.key == key; }),
        m_subs.end());
}

// ---------------------------------------------------------------------------
// emit — copy subscriber list under lock, then dispatch without lock
// ---------------------------------------------------------------------------
void EventBus::emit(nlohmann::json event)
{
    // Snapshot subscribers so we don't hold the mutex during dispatch.
    // Subscribers may themselves call subscribe/unsubscribe or emit.
    std::vector<Sub> snapshot;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        snapshot = m_subs;   // copy
    }

    for (const auto& sub : snapshot) {
        try {
            sub.cb(event);
        } catch (const std::exception& ex) {
            // Log but don't let one bad subscriber break others.
            std::cerr << "[EventBus] subscriber threw: " << ex.what() << "\n";
        } catch (...) {
            std::cerr << "[EventBus] subscriber threw unknown exception\n";
        }
    }
}

// ---------------------------------------------------------------------------
// makeEvent
// ---------------------------------------------------------------------------
nlohmann::json EventBus::makeEvent(const std::string& type,
                                    nlohmann::json     extra)
{
    nlohmann::json ev;
    ev["type"]      = type;
    ev["timestamp"] = nowISO8601();

    // Merge extra fields in
    if (extra.is_object()) {
        for (auto& [k, v] : extra.items()) {
            ev[k] = std::move(v);
        }
    }

    return ev;
}

} // namespace agent
