#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>

// A single tool definition loaded from tools_manifest.json
struct ToolDef {
    std::string name;
    std::string description;
    std::string category;       // e.g. "web_search", "maps", "weather"
    std::string script;         // Python script filename under tools/
    nlohmann::json input_schema;
};

// Loads tools_manifest.json and provides lookup / listing.
class ToolRegistry {
public:
    // Load from a manifest file (JSON array of tool objects).
    void load(const std::filesystem::path& manifest_path);

    // All tools, optionally filtered by category.
    std::vector<const ToolDef*> list(const std::string& category = "") const;

    // MCP tools/list response — grouped by category with a "category" field.
    nlohmann::json mcp_tools_list() const;

    // Find a single tool by name; returns nullptr if not found.
    const ToolDef* find(const std::string& name) const;

    // Base directory where tool Python scripts live.
    void set_tools_dir(const std::filesystem::path& dir) { m_tools_dir = dir; }
    const std::filesystem::path& tools_dir() const { return m_tools_dir; }

private:
    std::vector<ToolDef>          m_tools;
    std::filesystem::path         m_tools_dir;
};
