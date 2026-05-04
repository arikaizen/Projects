/**
 * @file tool_registry.h
 * @brief Tool schema definitions and validation for all nine graph DB tools.
 *
 * The ToolRegistry is the single source of truth for what tools this MCP server
 * advertises to the LLM host.  It serves two purposes:
 *
 *   1. Discovery  — listToolsJson() produces the JSON payload for tools/list
 *                   responses so the LLM knows each tool's name, description,
 *                   and required parameters.
 *
 *   2. Validation — validate() checks that a ToolCall received from the LLM
 *                   has all required arguments before the call is forwarded to
 *                   the graph DB.
 *
 * The nine tools mirror the nine commands of the graph DB wire protocol:
 *   create_entity, add_observation, create_relation,
 *   delete_entity, delete_observation, delete_relation,
 *   search_nodes, get_relations, read_graph
 */

#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include "mcp_types.h"    // ToolCall
#include <string>
#include <vector>

/**
 * @brief Describes one parameter of a tool.
 */
struct ToolParam {
    std::string name;        ///< Parameter name as used in the JSON arguments object
    std::string type;        ///< JSON Schema type, always "string" for this server
    std::string description; ///< Human-readable description shown to the LLM
    bool required;           ///< Whether the LLM must supply this argument
};

/**
 * @brief Describes one tool — its name, purpose, and parameter list.
 */
struct ToolSchema {
    std::string name;              ///< Tool identifier used in tools/call requests
    std::string description;       ///< Human-readable description shown to the LLM
    std::vector<ToolParam> params; ///< Ordered list of parameters
};

/**
 * @brief Owns all tool schemas and exposes discovery and validation services.
 *
 * The constructor registers all nine tools.  After construction the registry
 * is immutable — there is no add/remove API.
 */
class ToolRegistry {
public:
    /**
     * @brief Registers all nine graph DB tools.
     *
     * Each tool is described by its name, a helpful description for the LLM,
     * and a list of typed parameters with required/optional flags.
     */
    ToolRegistry();

    /**
     * @brief Serializes all registered tools into the JSON payload for a
     *        tools/list result.
     *
     * The returned string is a raw JSON object suitable for use as the
     * McpResponse::result field of a tools/list response:
     * @code
     *   {"tools": [{"name":"...", "description":"...", "inputSchema":{...}}, ...]}
     * @endcode
     *
     * Each tool's inputSchema follows JSON Schema draft-07:
     *   - type: "object"
     *   - properties: one entry per ToolParam
     *   - required: array of required parameter names
     *
     * @return Raw JSON string (no trailing newline).
     */
    std::string listToolsJson() const;

    /**
     * @brief Validates a ToolCall against its schema.
     *
     * Checks:
     *   - The tool name is registered.
     *   - All required parameters are present in call.arguments.
     *
     * Optional parameters that are missing are silently accepted.
     *
     * @param call  The tool invocation to validate.
     * @param err   Populated with a human-readable error if validation fails.
     * @return      true if the call is valid; false with err set if not.
     */
    bool validate(const ToolCall& call, std::string& err) const;

    /**
     * @brief Looks up a tool schema by name.
     *
     * @param name  Tool name to find.
     * @return      Pointer to the ToolSchema if found; nullptr if not registered.
     */
    const ToolSchema* find(const std::string& name) const;

private:
    /** All registered tools in insertion order (preserves tools/list ordering). */
    std::vector<ToolSchema> m_tools;
};

#endif // TOOL_REGISTRY_H
