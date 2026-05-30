#pragma once
#include <chrono>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

class AgentContext;

// Result recorded for every executed WorkItem.
struct WorkResult {
    std::string    item_id;
    std::string    item_name;
    std::string    item_kind;        // "Stage" or "Action"
    bool           success{false};
    nlohmann::json output;
    std::string    error;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds             duration{0};
    int            iteration{0};
    bool           ran_in_parallel{false};  // true when sibling items ran concurrently
    std::string    skipped_reason;          // non-empty if skipped due to failed dependency

    nlohmann::json toJson() const;
};

// Abstract base for everything the agent executes.
// Subclassed by Stage and Action; both live in the same queue.
class WorkItem {
public:
    enum class Kind { Stage, Action };

    std::string    id;
    std::string    name;
    nlohmann::json inputs;

    WorkItem(std::string id_, std::string name_, nlohmann::json inputs_ = {})
        : id(std::move(id_)), name(std::move(name_)), inputs(std::move(inputs_)) {}

    virtual ~WorkItem()                  = default;
    WorkItem(const WorkItem&)            = delete;
    WorkItem& operator=(const WorkItem&) = delete;

    virtual Kind        kind()        const = 0;
    virtual WorkResult  execute(AgentContext& ctx) = 0;
    virtual std::string description() const { return name; }

    // Returns the set of WorkItem ids that this item's inputs reference via
    // "$id" or "$id.field" syntax.  Used by BatchExecutor to build the DAG.
    std::set<std::string> dependencies() const;

    nlohmann::json toSummaryJson() const;

private:
    // Recursively scan a JSON value for strings matching ^\$([a-zA-Z0-9_]+)
    static void scanForRefs(const nlohmann::json& j, std::set<std::string>& out);
};

} // namespace agent
