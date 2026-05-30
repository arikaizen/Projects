#pragma once
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

class EventBus;

// Thread-safe shared key-value store accessible to all agents.
class Blackboard {
public:
    explicit Blackboard(EventBus* event_bus = nullptr);

    void write(const std::string& key, nlohmann::json value);
    std::optional<nlohmann::json> read(const std::string& key) const;
    void remove(const std::string& key);
    std::vector<std::string> keys(const std::string& prefix = "") const;
    bool contains(const std::string& key) const;

private:
    mutable std::mutex              m_mutex;
    std::map<std::string, nlohmann::json> m_data;
    EventBus*                       m_event_bus;
};

} // namespace agent
