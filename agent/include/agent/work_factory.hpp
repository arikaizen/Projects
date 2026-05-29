#pragma once
#include "work_item.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

// Metadata stored alongside each registered item type.
struct WorkItemSpec {
    std::string    name;
    std::string    description;
    WorkItem::Kind kind;
    nlohmann::json input_schema;  // JSON Schema describing the inputs object
};

// Registry + factory for WorkItems. The LLM names items by string; the factory
// builds them.  New types register themselves by calling registerItem() —
// no edits to this class are ever needed.
class WorkFactory {
public:
    using CreateFn = std::function<std::unique_ptr<WorkItem>(std::string id, nlohmann::json inputs)>;

    void registerItem(WorkItemSpec spec, CreateFn fn);

    std::unique_ptr<WorkItem> create(const std::string& name,
                                     const std::string& id,
                                     const nlohmann::json& inputs) const;

    bool isRegistered(const std::string& name) const;

    // All registered specs (for catalog generation).
    std::vector<WorkItemSpec> listSpecs() const;

    // Serialised catalog for the {{CATALOG}} prompt placeholder.
    nlohmann::json toCatalogJson() const;

    // Input schema for a specific item (for plan validation).
    const nlohmann::json* inputSchema(const std::string& name) const;

private:
    mutable std::shared_mutex             m_mutex;  // shared_lock for reads, unique_lock for writes
    std::map<std::string, WorkItemSpec>   m_specs;
    std::map<std::string, CreateFn>       m_fns;
};

} // namespace agent
