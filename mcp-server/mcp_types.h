/**
 * @file mcp_types.h
 * @brief Core MCP message structs shared across all components.
 *
 * These are the three fundamental data types that flow through the MCP server:
 *   - McpRequest  : a decoded JSON-RPC 2.0 request from the LLM host (stdin)
 *   - McpResponse : a JSON-RPC 2.0 response to write back to the LLM host (stdout)
 *   - ToolCall    : the parsed content of a tools/call request's params field
 *
 * All string fields that hold JSON values (id, params, result, error) store the
 * raw JSON representation rather than a decoded form so they can be embedded
 * directly into outgoing JSON without re-serialization.
 */

#ifndef MCP_TYPES_H
#define MCP_TYPES_H

#include <string>
#include <unordered_map>

/**
 * @brief A parsed JSON-RPC 2.0 request received on stdin from the MCP host.
 *
 * The MCP protocol is newline-delimited: each line on stdin is exactly one
 * JSON-RPC 2.0 message.  After parsing, the fields are:
 *
 *   jsonrpc : always "2.0"
 *   id      : raw JSON id value — "1" for integer 1, "\"abc\"" for string "abc",
 *             "null" if the id field is absent or explicitly null.
 *             A null id means the message is a *notification* — the server must
 *             NOT send a response.
 *   method  : decoded method string, e.g. "initialize", "tools/list", "tools/call"
 *   params  : raw JSON value of the params field (an object or empty "{}")
 */
struct McpRequest {
    std::string jsonrpc; ///< JSON-RPC version string, always "2.0"
    std::string id;      ///< Raw JSON id; "null" for notifications
    std::string method;  ///< Decoded method name
    std::string params;  ///< Raw JSON params object
};

/**
 * @brief A JSON-RPC 2.0 response to be serialized and written to stdout.
 *
 * Exactly one of result or error must be non-empty.  Both being empty is
 * invalid JSON-RPC; both being non-empty is also invalid — the caller must
 * ensure only one is set.
 *
 *   id     : must echo the id from the originating McpRequest verbatim
 *   result : raw JSON result value (object, array, string, etc.)
 *   error  : raw JSON error object {"code":N,"message":"..."}
 */
struct McpResponse {
    std::string jsonrpc; ///< Always "2.0"
    std::string id;      ///< Echoed raw JSON id from the request
    std::string result;  ///< Raw JSON result — set on success, empty on error
    std::string error;   ///< Raw JSON error object — set on failure, empty on success
};

/**
 * @brief A parsed tools/call invocation extracted from a request's params field.
 *
 * When the LLM wants to call a tool it sends:
 * @code
 *   {"name": "create_entity", "arguments": {"name": "Alice", "type": "person"}}
 * @endcode
 *
 * After parsing, name holds "create_entity" and arguments holds
 * {"name" → "Alice", "type" → "person"} as decoded C++ strings.
 */
struct ToolCall {
    /** Name of the tool being invoked, e.g. "create_entity". */
    std::string name;

    /**
     * Decoded argument key → value pairs.
     * All values are strings (the graph DB schema only uses string arguments).
     */
    std::unordered_map<std::string, std::string> arguments;
};

#endif // MCP_TYPES_H
