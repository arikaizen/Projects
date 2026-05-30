// work_factory.cpp — WorkFactory implementation
//
// Thread-safety: std::shared_mutex guards the spec/fn maps.
//   - reads (create, isRegistered, listSpecs, toCatalogJson, inputSchema) use
//     std::shared_lock — many concurrent agent threads may read simultaneously.
//   - writes (registerItem) use std::unique_lock — only happens at startup.
//
// NOTE: inputSchema() returns a pointer into m_specs. After startup the map is
// never mutated, making the raw pointer permanently stable.
#include "agent/work_factory.hpp"

#include <stdexcept>

namespace agent {

void WorkFactory::registerItem(WorkItemSpec spec, CreateFn fn)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    if (m_specs.count(spec.name)) {
        throw std::invalid_argument(
            "WorkFactory: item '" + spec.name + "' is already registered");
    }
    std::string key = spec.name;
    m_specs[key]    = std::move(spec);
    m_fns[key]      = std::move(fn);
}

std::unique_ptr<WorkItem> WorkFactory::create(const std::string& name,
                                               const std::string& id,
                                               const nlohmann::json& inputs) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_fns.find(name);
    if (it == m_fns.end()) {
        throw std::runtime_error("Unknown WorkItem: " + name);
    }
    return it->second(id, inputs);
}

bool WorkFactory::isRegistered(const std::string& name) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_specs.count(name) > 0;
}

std::vector<WorkItemSpec> WorkFactory::listSpecs() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<WorkItemSpec> result;
    result.reserve(m_specs.size());
    for (const auto& [key, spec] : m_specs)
        result.push_back(spec);
    return result;
}

nlohmann::json WorkFactory::toCatalogJson() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [key, spec] : m_specs) {
        arr.push_back({
            {"name",         spec.name},
            {"description",  spec.description},
            {"kind",         spec.kind == WorkItem::Kind::Stage ? "Stage" : "Action"},
            {"input_schema", spec.input_schema}
        });
    }
    return arr;
}

const nlohmann::json* WorkFactory::inputSchema(const std::string& name) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);

    auto it = m_specs.find(name);
    if (it == m_specs.end()) return nullptr;
    return &it->second.input_schema;  // stable after startup
}

} // namespace agent
