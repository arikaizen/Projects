/**
 * @file tool_registry.cpp
 * @brief Registration of all nine graph DB tools and schema serialization.
 *
 * Tool definitions live here rather than in a config file so that the schemas
 * are always in sync with the translation table in db_client.cpp — both must
 * agree on parameter names and the set of required arguments.
 */

#include "tool_registry.h"
#include "json_rpc.h"   // jsonEscape — used when building the tools JSON

#include <cstdio>       // std::snprintf

// ---------------------------------------------------------------------------
// Constructor — register all nine tools
// ---------------------------------------------------------------------------

ToolRegistry::ToolRegistry() {
    // Helper lambda to build a ToolSchema and append it to m_tools.
    auto add = [&](const std::string& name,
                   const std::string& desc,
                   std::vector<ToolParam> params) {
        m_tools.push_back({name, desc, std::move(params)});
    };

    // -----------------------------------------------------------------------
    // 1. create_entity — create a new node in the graph
    // -----------------------------------------------------------------------
    add("create_entity",
        "Create a new named entity in the knowledge graph.",
        {
            {"name", "string", "Unique name for the entity (primary key)", true},
            {"type", "string", "Category label, e.g. person, company, location", true}
        });

    // -----------------------------------------------------------------------
    // 2. add_observation — attach a free-text fact to an entity
    // -----------------------------------------------------------------------
    add("add_observation",
        "Append a free-text observation (a known fact) to an existing entity.",
        {
            {"entity",      "string", "Name of the entity to annotate", true},
            {"observation", "string", "Free-text fact, e.g. \"Speaks Spanish\"", true}
        });

    // -----------------------------------------------------------------------
    // 3. create_relation — add a directed edge between two entities
    // -----------------------------------------------------------------------
    add("create_relation",
        "Create a directed, labelled relationship between two existing entities.",
        {
            {"from",         "string", "Name of the source entity", true},
            {"relationType", "string", "Relationship label, e.g. works_at, knows", true},
            {"to",           "string", "Name of the target entity", true}
        });

    // -----------------------------------------------------------------------
    // 4. delete_entity — remove a node and cascade to its edges
    // -----------------------------------------------------------------------
    add("delete_entity",
        "Delete an entity and automatically remove all relations that reference it.",
        {
            {"name", "string", "Name of the entity to delete", true}
        });

    // -----------------------------------------------------------------------
    // 5. delete_observation — remove one specific observation from an entity
    // -----------------------------------------------------------------------
    add("delete_observation",
        "Remove a specific observation from an entity (exact text match, first occurrence).",
        {
            {"entity",      "string", "Name of the entity that owns the observation", true},
            {"observation", "string", "Exact text of the observation to remove", true}
        });

    // -----------------------------------------------------------------------
    // 6. delete_relation — remove one specific directed edge
    // -----------------------------------------------------------------------
    add("delete_relation",
        "Remove the directed relation identified by the exact (from, relationType, to) triple.",
        {
            {"from",         "string", "Name of the source entity", true},
            {"relationType", "string", "Relationship label to match", true},
            {"to",           "string", "Name of the target entity", true}
        });

    // -----------------------------------------------------------------------
    // 7. search_nodes — find entities by text
    // -----------------------------------------------------------------------
    add("search_nodes",
        "Search for entities whose name, type, or any observation contains the query "
        "string (case-insensitive substring match).",
        {
            {"query", "string", "Substring to search for", true}
        });

    // -----------------------------------------------------------------------
    // 8. get_relations — retrieve all edges for one entity
    // -----------------------------------------------------------------------
    add("get_relations",
        "Get all relations where the entity appears as the source or target, "
        "providing a complete neighbourhood view.",
        {
            {"entity", "string", "Name of the entity whose relations to retrieve", true}
        });

    // -----------------------------------------------------------------------
    // 9. read_graph — dump the entire graph
    // -----------------------------------------------------------------------
    add("read_graph",
        "Read the complete knowledge graph — all entities and all relations.",
        {}); // no parameters
}

// ---------------------------------------------------------------------------
// listToolsJson
// ---------------------------------------------------------------------------

/**
 * Builds the JSON Schema inputSchema for one tool.
 *
 * Shape:
 *   {"type":"object","properties":{"param":{"type":"string","description":"..."},...},
 *    "required":["param",...]}
 *
 * For tools with no parameters (read_graph) the properties object is empty
 * and the required array is omitted.
 */
static std::string buildInputSchema(const std::vector<ToolParam>& params) {
    std::string schema = "{\"type\":\"object\",\"properties\":{";

    bool firstProp = true;
    std::string required;

    for (const auto& p : params) {
        if (!firstProp) schema += ",";
        schema += "\"" + p.name + "\":{\"type\":\"";
        schema += p.type;
        schema += "\",\"description\":\"";
        schema += json_rpc::jsonEscape(p.description);
        schema += "\"}";
        firstProp = false;

        if (p.required) {
            if (!required.empty()) required += ",";
            required += "\"" + p.name + "\"";
        }
    }

    schema += "}"; // close properties

    if (!required.empty()) {
        schema += ",\"required\":[" + required + "]";
    }

    schema += "}"; // close inputSchema object
    return schema;
}

std::string ToolRegistry::listToolsJson() const {
    std::string result = "{\"tools\":[";

    for (size_t i = 0; i < m_tools.size(); ++i) {
        if (i > 0) result += ",";
        const ToolSchema& t = m_tools[i];

        result += "{\"name\":\"";
        result += json_rpc::jsonEscape(t.name);
        result += "\",\"description\":\"";
        result += json_rpc::jsonEscape(t.description);
        result += "\",\"inputSchema\":";
        result += buildInputSchema(t.params);
        result += "}"; // close tool object
    }

    result += "]}"; // close tools array and result object
    return result;
}

// ---------------------------------------------------------------------------
// validate
// ---------------------------------------------------------------------------

bool ToolRegistry::validate(const ToolCall& call, std::string& err) const {
    const ToolSchema* schema = find(call.name);
    if (!schema) {
        err = "Unknown tool: " + call.name;
        return false;
    }

    // Check that every required parameter has been supplied.
    for (const auto& param : schema->params) {
        if (param.required && !call.arguments.count(param.name)) {
            err = "Missing required argument \"" + param.name +
                  "\" for tool \"" + call.name + "\"";
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// find
// ---------------------------------------------------------------------------

const ToolSchema* ToolRegistry::find(const std::string& name) const {
    // Linear scan over 9 tools — a map would add complexity for no real gain.
    for (const auto& t : m_tools) {
        if (t.name == name) return &t;
    }
    return nullptr;
}
