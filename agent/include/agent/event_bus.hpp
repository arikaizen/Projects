#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

using EventCallback = std::function<void(const nlohmann::json& event)>;

// Simple synchronous event bus.  Callbacks fire on the calling thread
// (engine threads).  GUI consumers must marshal to their UI thread.
class EventBus {
public:
    // key is used for unsubscription; pass the address of your callback or any
    // stable pointer that uniquely identifies the subscriber.
    void subscribe(EventCallback cb, void* key = nullptr);
    void unsubscribe(void* key);

    // Fire synchronously — does NOT hold the mutex during dispatch so that
    // subscribers may re-enter (e.g. call AgentManager methods).
    void emit(nlohmann::json event);

    // Build a typed event envelope.
    static nlohmann::json makeEvent(const std::string& type,
                                    nlohmann::json extra = {});

private:
    struct Sub { EventCallback cb; void* key; };
    mutable std::mutex  m_mutex;
    std::vector<Sub>    m_subs;
};

} // namespace agent
