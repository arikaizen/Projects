/**
 * @file json_rpc.h
 * @brief JSON-RPC 2.0 parsing and formatting — no third-party libraries.
 *
 * This module handles all JSON serialization and deserialization for the MCP
 * server.  It implements a minimal recursive-descent JSON parser sufficient for
 * the MCP wire format and provides formatting helpers for every outgoing message
 * shape.
 *
 * Design constraints
 * ------------------
 * - No third-party JSON library.  All parsing is hand-written.
 * - The parser is NOT a general-purpose JSON parser.  It handles the specific
 *   shapes produced by well-behaved MCP hosts and is robust against malformed
 *   input (returns empty/default structs rather than crashing).
 * - All output functions produce compact single-line JSON (no pretty-printing)
 *   because the MCP protocol requires exactly one JSON object per line.
 *
 * Encoding contract
 * -----------------
 * - Incoming strings (tool names, argument values) are fully decoded:
 *   \\n → newline, \\uXXXX → UTF-8 bytes, etc.
 * - Outgoing strings are escaped via jsonEscape() before embedding in JSON.
 * - The `id`, `params`, `result`, and `error` fields in McpRequest/McpResponse
 *   carry raw JSON values and are embedded verbatim — no extra escaping.
 */

#ifndef JSON_RPC_H
#define JSON_RPC_H

#include "mcp_types.h"
#include <string>

namespace json_rpc {

/**
 * @brief Parses one JSON-RPC 2.0 request line into an McpRequest.
 *
 * The input is expected to be a single JSON object with no surrounding
 * whitespace other than a possible trailing newline (already stripped by
 * std::getline).  On parse failure the returned McpRequest has an empty
 * method field; callers must check for this and return a -32700 error.
 *
 * The `id` field in the returned struct holds the raw JSON value:
 *   - integer 1   → id = "1"
 *   - string "x"  → id = "\"x\""
 *   - null / absent → id = "null"
 *
 * @param line  A single trimmed line from stdin.
 * @return      Decoded McpRequest; method is empty on fatal parse error.
 */
McpRequest parseRequest(const std::string& line);

/**
 * @brief Serializes an McpResponse into a single JSON-RPC 2.0 response line.
 *
 * If response.error is non-empty, the error field is used.
 * Otherwise result is used.  The returned string does NOT include a trailing
 * newline — the caller (McpServer) appends "\n" and flushes stdout.
 *
 * @param response  The response to serialize.
 * @return          A single-line JSON string without trailing newline.
 */
std::string formatResponse(const McpResponse& response);

/**
 * @brief Builds a JSON-RPC 2.0 error response string.
 *
 * Standard error codes:
 *   -32700  Parse error
 *   -32601  Method not found
 *   -32602  Invalid params
 *   -32603  Internal error
 *
 * @param id       Raw JSON id to echo back (use "null" for parse errors where
 *                 no id could be extracted).
 * @param code     JSON-RPC error code (negative integer).
 * @param message  Human-readable error description; will be JSON-escaped.
 * @return         A single-line JSON-RPC error response without trailing newline.
 */
std::string formatError(const std::string& id, int code, const std::string& message);

/**
 * @brief Builds a JSON-RPC 2.0 notification string (no id field).
 *
 * Notifications are one-way messages that do not expect a response.
 * Used by the server to send "notifications/initialized" after startup.
 *
 * @param method  The notification method name.
 * @param params  Raw JSON params value (object or "{}").
 * @return        A single-line JSON notification string without trailing newline.
 */
std::string formatNotification(const std::string& method, const std::string& params);

/**
 * @brief Parses a tools/call params JSON object into a ToolCall struct.
 *
 * The params object has this shape:
 * @code
 *   {"name": "create_entity", "arguments": {"name": "Alice", "type": "person"}}
 * @endcode
 *
 * After parsing:
 *   - call.name = "create_entity"
 *   - call.arguments = {"name" → "Alice", "type" → "person"}
 *
 * All string values are fully decoded (JSON escape sequences resolved).
 * On parse failure, the returned ToolCall has an empty name field.
 *
 * @param paramsJson  Raw JSON params string from a tools/call request.
 * @return            Decoded ToolCall; name is empty on failure.
 */
ToolCall parseToolCall(const std::string& paramsJson);

/**
 * @brief Escapes a C++ string for safe embedding in a JSON string value.
 *
 * The returned value does NOT include surrounding quote characters.
 * Handles: \" \\ \n \r \t and \\uXXXX for control characters < 0x20.
 * UTF-8 bytes >= 0x80 pass through unchanged.
 *
 * @param s  Raw C++ string to escape.
 * @return   JSON-safe escaped form, suitable for insertion between quotes.
 */
std::string jsonEscape(const std::string& s);

} // namespace json_rpc

#endif // JSON_RPC_H
