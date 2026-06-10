#include "tool_registry.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>

void ToolRegistry::load(const std::filesystem::path& manifest_path) {
    std::ifstream f(manifest_path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open manifest: " + manifest_path.string());

    nlohmann::json doc;
    f >> doc;
    if (!doc.is_array())
        throw std::runtime_error("tools_manifest.json must be a JSON array");

    m_tools.clear();
    for (auto& obj : doc) {
        ToolDef td;
        td.name         = obj.value("name",         std::string{});
        td.description  = obj.value("description",  std::string{});
        td.category     = obj.value("category",     std::string{"misc"});
        td.script       = obj.value("script",       td.name + ".py");
        td.input_schema = obj.value("inputSchema",  nlohmann::json::object());
        if (!td.name.empty()) m_tools.push_back(std::move(td));
    }
}

std::vector<const ToolDef*> ToolRegistry::list(const std::string& category) const {
    std::vector<const ToolDef*> out;
    for (auto& t : m_tools) {
        if (category.empty() || t.category == category)
            out.push_back(&t);
    }
    return out;
}

nlohmann::json ToolRegistry::mcp_tools_list() const {
    // Group tools by category; each tool also carries a "category" annotation
    // so the model / agent can filter precisely.
    nlohmann::json arr = nlohmann::json::array();
    for (auto& t : m_tools) {
        arr.push_back({
            {"name",        t.name},
            {"description", t.description},
            {"category",    t.category},
            {"inputSchema", t.input_schema},
        });
    }
    return arr;
}

const ToolDef* ToolRegistry::find(const std::string& name) const {
    for (auto& t : m_tools) {
        if (t.name == name) return &t;
    }
    return nullptr;
}
