/**
 * @file json_rpc.cpp
 * @brief Hand-written JSON-RPC 2.0 parser and formatter.
 *
 * The parser uses a recursive-descent approach via a local Parser struct.
 * It handles all JSON value types needed for the MCP protocol:
 *   - strings   (with full escape sequence decoding incl. \uXXXX → UTF-8)
 *   - numbers   (stored as raw substring)
 *   - objects   (parsed into std::unordered_map<string, raw_value>)
 *   - arrays    (skipped as raw substrings — not needed for our shapes)
 *   - null      (stored as raw "null")
 *   - true/false (stored as raw "true"/"false")
 *
 * Only parseObject() is used recursively; arrays are treated as opaque raw
 * values since the MCP shapes we handle don't require array iteration.
 */

#include "json_rpc.h"

#include <unordered_map>
#include <cctype>   // std::isspace, std::isdigit
#include <cstdio>   // std::snprintf
#include <string>

namespace json_rpc {

// =============================================================================
// Anonymous namespace — internal parser implementation
// =============================================================================
namespace {

/**
 * @brief Minimal JSON parser that reads from a const string reference.
 *
 * pos is the current read cursor.  All public methods advance pos as they
 * consume input; on entry they assume pos is already at the start of the
 * relevant token (callers call skipWS() first where needed).
 */
struct Parser {
    const std::string& s; ///< The JSON text being parsed
    size_t pos;           ///< Current read position

    explicit Parser(const std::string& str, size_t start = 0)
        : s(str), pos(start) {}

    /** Advances pos past any ASCII whitespace characters. */
    void skipWS() {
        while (pos < s.size() &&
               std::isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
    }

    /**
     * Parses a JSON string starting at pos (must be at the opening '"').
     * Returns the decoded C++ string and leaves pos just past the closing '"'.
     *
     * Handles all standard JSON escape sequences:
     *   \" \\ \/ \n \r \t  and  \uXXXX (decoded to UTF-8).
     */
    std::string parseString() {
        if (pos >= s.size() || s[pos] != '"') return "";
        ++pos; // skip opening '"'

        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos; // consume the backslash
                switch (s[pos]) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'u':
                        // \uXXXX — decode 4 hex digits into a Unicode code
                        // point and emit as UTF-8.
                        if (pos + 4 < s.size()) {
                            unsigned int cp = 0;
                            for (int j = 1; j <= 4; ++j) {
                                unsigned char h =
                                    static_cast<unsigned char>(s[pos + j]);
                                cp <<= 4;
                                if      (h >= '0' && h <= '9') cp += h - '0';
                                else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                                else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            }
                            pos += 4; // skip the 4 hex digits

                            // Encode the code point as UTF-8.
                            if (cp < 0x80) {
                                result += static_cast<char>(cp);
                            } else if (cp < 0x800) {
                                result += static_cast<char>(0xC0 | (cp >> 6));
                                result += static_cast<char>(0x80 | (cp & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (cp >> 12));
                                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (cp & 0x3F));
                            }
                        }
                        break;
                    default:
                        result += s[pos]; // unknown escape — pass character through
                        break;
                }
            } else {
                result += s[pos]; // ordinary character
            }
            ++pos;
        }
        if (pos < s.size()) ++pos; // skip closing '"'
        return result;
    }

    /**
     * Skips any JSON value starting at pos, returning the raw substring.
     * Does NOT decode escape sequences — callers that need the decoded value
     * of a string must call parseString() directly.
     *
     * For strings: scans past escape sequences correctly so embedded '"' chars
     * inside strings do not prematurely terminate the scan.
     * For objects/arrays: uses depth tracking so nested braces/brackets are
     * handled without recursion.
     * For atoms (true, false, null, numbers): advances past the token.
     */
    std::string skipValue() {
        skipWS();
        if (pos >= s.size()) return "";

        size_t start = pos;

        if (s[pos] == '"') {
            // String: scan past backslash-escaped characters.
            ++pos;
            while (pos < s.size() && s[pos] != '"') {
                // A backslash escapes the very next character, whatever it is.
                // This correctly handles \" (escaped quote) and \\ (escaped backslash).
                if (s[pos] == '\\' && pos + 1 < s.size()) pos += 2;
                else ++pos;
            }
            if (pos < s.size()) ++pos; // skip closing '"'

        } else if (s[pos] == '{' || s[pos] == '[') {
            // Object or array: track nesting depth.
            char open  = s[pos];
            char close = (open == '{') ? '}' : ']';
            int depth  = 1;
            ++pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '"') {
                    // String inside the object/array — must scan past it
                    // properly so a '"' inside a string value doesn't deceive
                    // the depth tracker.
                    ++pos;
                    while (pos < s.size() && s[pos] != '"') {
                        if (s[pos] == '\\' && pos + 1 < s.size()) pos += 2;
                        else ++pos;
                    }
                    if (pos < s.size()) ++pos;
                } else if (s[pos] == open) {
                    ++depth; ++pos;
                } else if (s[pos] == close) {
                    --depth; ++pos;
                } else {
                    ++pos;
                }
            }

        } else if (s[pos] == 't') { pos += 4; }   // true
        else if  (s[pos] == 'f') { pos += 5; }   // false
        else if  (s[pos] == 'n') { pos += 4; }   // null
        else {
            // Number: consume digits, sign, decimal point, exponent.
            while (pos < s.size() &&
                   (std::isdigit(static_cast<unsigned char>(s[pos])) ||
                    s[pos] == '-' || s[pos] == '+' ||
                    s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E'))
                ++pos;
        }

        return s.substr(start, pos - start);
    }

    /**
     * Parses a JSON object starting at pos (must be at '{').
     * Returns a map of decoded_key → raw_value_substring.
     *
     * The raw value for a string field still contains its surrounding quotes
     * and any escape sequences — call rawToString() to decode it.
     */
    std::unordered_map<std::string, std::string> parseObject() {
        std::unordered_map<std::string, std::string> result;
        skipWS();
        if (pos >= s.size() || s[pos] != '{') return result;
        ++pos; // skip '{'

        while (true) {
            skipWS();
            if (pos >= s.size() || s[pos] == '}') {
                if (pos < s.size()) ++pos; // skip '}'
                break;
            }
            if (s[pos] == ',') { ++pos; continue; } // skip comma between members
            if (s[pos] != '"') break;                // malformed — stop

            std::string key = parseString(); // decoded key name
            skipWS();
            if (pos >= s.size() || s[pos] != ':') break; // malformed
            ++pos; // skip ':'
            skipWS();
            std::string rawValue = skipValue(); // raw JSON value
            result[key] = rawValue;
        }
        return result;
    }
};

// ---------------------------------------------------------------------------
// Helper: convert a raw JSON value to a decoded C++ string.
//
//   "\"hello\"" → "hello"   (JSON string literal → decoded string)
//   "42"        → "42"      (number → its digit string)
//   "null"      → ""        (null → empty string)
// ---------------------------------------------------------------------------
std::string rawToString(const std::string& raw) {
    if (raw.empty() || raw == "null") return "";
    if (!raw.empty() && raw.front() == '"') {
        // It's a JSON string — decode it using the Parser's parseString().
        Parser p(raw);
        return p.parseString();
    }
    // Number or boolean — return the raw digits/keyword directly.
    return raw;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

McpRequest parseRequest(const std::string& line) {
    McpRequest req;
    Parser p(line);
    auto fields = p.parseObject();

    // jsonrpc and method are string fields — decode them.
    req.jsonrpc = rawToString(fields.count("jsonrpc") ? fields.at("jsonrpc") : "");
    req.method  = rawToString(fields.count("method")  ? fields.at("method")  : "");

    // id is kept as raw JSON so it can be echoed verbatim in the response.
    // Absent id (notification) or explicit null both map to "null".
    req.id = fields.count("id") ? fields.at("id") : "null";

    // params is kept as raw JSON (typically an object) for downstream parsing.
    req.params = fields.count("params") ? fields.at("params") : "{}";

    return req;
}

std::string formatResponse(const McpResponse& response) {
    // Produce: {"jsonrpc":"2.0","id":<id>,"result":<result>}
    //      or: {"jsonrpc":"2.0","id":<id>,"error":<error>}
    std::string out = "{\"jsonrpc\":\"2.0\",\"id\":";
    out += response.id; // raw JSON — already valid

    if (!response.error.empty()) {
        out += ",\"error\":";
        out += response.error; // raw JSON error object
    } else {
        out += ",\"result\":";
        out += response.result.empty() ? "null" : response.result;
    }

    out += "}";
    return out;
}

std::string formatError(const std::string& id, int code, const std::string& message) {
    std::string out = "{\"jsonrpc\":\"2.0\",\"id\":";
    out += id;
    out += ",\"error\":{\"code\":";
    out += std::to_string(code);
    out += ",\"message\":\"";
    out += jsonEscape(message);
    out += "\"}}";
    return out;
}

std::string formatNotification(const std::string& method, const std::string& params) {
    // Notifications have no "id" field — they are fire-and-forget messages.
    std::string out = "{\"jsonrpc\":\"2.0\",\"method\":\"";
    out += jsonEscape(method);
    out += "\",\"params\":";
    out += params.empty() ? "{}" : params;
    out += "}";
    return out;
}

ToolCall parseToolCall(const std::string& paramsJson) {
    ToolCall call;

    // Parse the outer params object: {"name": "...", "arguments": {...}}
    Parser p(paramsJson);
    auto fields = p.parseObject();

    call.name = rawToString(fields.count("name") ? fields.at("name") : "");

    // Parse the nested "arguments" object into the key→value map.
    if (fields.count("arguments")) {
        const std::string& argsRaw = fields.at("arguments");
        if (!argsRaw.empty() && argsRaw.front() == '{') {
            Parser argParser(argsRaw);
            auto argFields = argParser.parseObject();
            for (const auto& [k, v] : argFields) {
                // Decode each argument value from its raw JSON representation.
                call.arguments[k] = rawToString(v);
            }
        }
    }

    return call;
}

std::string jsonEscape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control characters must be \uXXXX encoded in JSON.
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c); // printable ASCII or UTF-8 byte
                }
                break;
        }
    }
    return result;
}

} // namespace json_rpc
