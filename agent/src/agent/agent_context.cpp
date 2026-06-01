// agent_context.cpp — AgentContext implementation
#include "agent/agent_context.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace agent {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
AgentContext::AgentContext(
    AgentConfig                    config,
    std::shared_ptr<LLMClient>     llm,
    std::shared_ptr<WorkFactory>   factory,
    std::shared_ptr<PromptLoader>  prompt_loader,
    std::shared_ptr<MemoryBackend> memory,
    Blackboard*                    blackboard,
    MessageInbox*                  inbox,
    EventBus*                      event_bus,
    AgentManager*                  manager,
    PlanCache*                     plan_cache)
    : m_config       (std::move(config))
    , m_llm          (std::move(llm))
    , m_factory      (std::move(factory))
    , m_prompt_loader(std::move(prompt_loader))
    , m_memory       (std::move(memory))
    , m_blackboard   (blackboard)
    , m_inbox        (inbox)
    , m_event_bus    (event_bus)
    , m_manager      (manager)
    , m_plan_cache   (plan_cache)
{}

// ---------------------------------------------------------------------------
// Queue — push / injectFromOutside / pop / try_pop / wakeLoop
// ---------------------------------------------------------------------------

void AgentContext::push(std::unique_ptr<WorkItem> item, Position pos)
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        if (pos == Position::Front) {
            m_queue.push_front(std::move(item));
        } else {
            m_queue.push_back(std::move(item));
        }
    }
    m_queue_cv.notify_one();
}

void AgentContext::injectFromOutside(std::unique_ptr<WorkItem> item, Position pos)
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        if (pos == Position::Front) {
            m_queue.push_front(std::move(item));
        } else {
            m_queue.push_back(std::move(item));
        }
    }
    m_queue_cv.notify_one();
}

std::unique_ptr<WorkItem> AgentContext::pop()
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);

    // Wait for an item, but only up to a bounded idle grace period.  If the
    // queue stays empty for the whole grace window (and we were not cancelled
    // or asked to stop), the agent has run out of work → return nullptr so the
    // loop terminates with QueueEmpty.  Real-time injection still works: items
    // injected while the agent is busy executing a batch are picked up by the
    // try_pop() drain in the same iteration; the grace window only governs how
    // long an *idle* agent waits before concluding it is done.
    const auto grace = std::chrono::milliseconds(m_idle_grace_ms);
    bool have_item = m_queue_cv.wait_for(lock, grace, [this] {
        return !m_queue.empty() || cancellation_flag.load() || should_stop;
    });

    if (!have_item || m_queue.empty()) {
        return nullptr;  // idle timeout, cancellation, or should_stop
    }

    auto item = std::move(m_queue.front());
    m_queue.pop_front();
    return item;
}

// Non-blocking pop — returns nullptr immediately if queue is empty.
// Used by the agent loop to drain all items pushed by a stage in one round.
std::unique_ptr<WorkItem> AgentContext::try_pop()
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    if (m_queue.empty()) return nullptr;
    auto item = std::move(m_queue.front());
    m_queue.pop_front();
    return item;
}

void AgentContext::wakeLoop()
{
    m_queue_cv.notify_all();
}

bool AgentContext::queueEmpty() const
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    return m_queue.empty();
}

std::size_t AgentContext::queueSize() const
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    return m_queue.size();
}

nlohmann::json AgentContext::queueSummaryJson() const
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& item : m_queue) {
        arr.push_back({
            {"id",   item->id},
            {"name", item->name},
            {"kind", item->kind() == WorkItem::Kind::Stage ? "Stage" : "Action"}
        });
    }
    return arr;
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

void AgentContext::recordResult(WorkResult result)
{
    std::unique_lock<std::mutex> lock(m_history_mutex);
    m_history.push_back(std::move(result));
}

const WorkResult* AgentContext::lastResult() const
{
    std::unique_lock<std::mutex> lock(m_history_mutex);
    if (m_history.empty()) return nullptr;
    return &m_history.back();
}

const WorkResult* AgentContext::resultById(const std::string& id) const
{
    std::unique_lock<std::mutex> lock(m_history_mutex);
    for (const auto& r : m_history) {
        if (r.item_id == id) return &r;
    }
    return nullptr;
}

nlohmann::json AgentContext::historySummaryJson(int max_entries) const
{
    std::unique_lock<std::mutex> lock(m_history_mutex);
    nlohmann::json arr = nlohmann::json::array();

    if (m_history.empty()) return arr;

    int  count = std::min(max_entries, static_cast<int>(m_history.size()));
    auto start = static_cast<int>(m_history.size()) - count;

    // Most recent first
    for (int i = static_cast<int>(m_history.size()) - 1; i >= start; --i) {
        const auto& r = m_history[static_cast<std::size_t>(i)];
        arr.push_back(r.toJson());
    }
    return arr;
}

const std::vector<WorkResult>& AgentContext::history() const
{
    // NOTE: caller must not rely on locking; this is a read accessor
    // intended for single-threaded contexts (e.g. tests, synthesis after run).
    return m_history;
}

// ---------------------------------------------------------------------------
// Reference resolution
// ---------------------------------------------------------------------------

// Pattern: starts with $, then an identifier, then optionally .field
static const std::regex kFullRefRe(
    R"(^\$([A-Za-z0-9_]+)(?:\.([A-Za-z0-9_]+))?$)");

nlohmann::json AgentContext::resolveRefString(const std::string& ref) const
{
    std::smatch m;
    if (!std::regex_match(ref, m, kFullRefRe)) {
        // Not a reference — return as-is
        return ref;
    }

    std::string id_part    = m[1].str();
    std::string field_part = m[2].str();  // empty if no .field

    const WorkResult* result = resultById(id_part);
    if (!result) {
        throw std::runtime_error("Unresolved reference: " + ref +
                                 " (no result with id '" + id_part + "')");
    }

    if (field_part.empty()) {
        return result->output;
    }

    if (!result->output.is_object() || !result->output.contains(field_part)) {
        throw std::runtime_error("Unresolved reference: " + ref +
                                 " (field '" + field_part +
                                 "' not found in output of '" + id_part + "')");
    }
    return result->output[field_part];
}

nlohmann::json AgentContext::resolveReferences(const nlohmann::json& inputs) const
{
    if (inputs.is_string()) {
        const std::string& s = inputs.get<std::string>();
        // Only resolve if the string looks like a reference
        if (!s.empty() && s[0] == '$') {
            return resolveRefString(s);
        }
        return inputs;
    }

    if (inputs.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& elem : inputs) {
            out.push_back(resolveReferences(elem));
        }
        return out;
    }

    if (inputs.is_object()) {
        nlohmann::json out = nlohmann::json::object();
        for (const auto& [key, val] : inputs.items()) {
            out[key] = resolveReferences(val);
        }
        return out;
    }

    return inputs;  // number, bool, null — return unchanged
}

// ---------------------------------------------------------------------------
// idExists
// ---------------------------------------------------------------------------
bool AgentContext::idExists(const std::string& id) const
{
    // Check history
    {
        std::unique_lock<std::mutex> lock(m_history_mutex);
        for (const auto& r : m_history) {
            if (r.item_id == id) return true;
        }
    }

    // Check queue
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        for (const auto& item : m_queue) {
            if (item->id == id) return true;
        }
    }

    return false;
}

} // namespace agent
