/**
 * @file persistence.cpp
 * @brief Implementation of JSONL-based graph persistence.
 *
 * All JSON handling is done with hand-written string operations — no third-party
 * library is involved.  The format is intentionally simple so that the parser
 * (extractString / extractArray) can rely on the exact byte layout produced by
 * save(), rather than needing to handle arbitrary valid JSON.
 */

#include "persistence.h"

#include <fstream>
#include <stdexcept>
#include <cstdio>   // std::snprintf

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Persistence::Persistence(const std::string& filePath)
    : m_filePath(filePath) {}

// ---------------------------------------------------------------------------
// Private helpers — JSON serialization
// ---------------------------------------------------------------------------

/**
 * Converts a raw C++ string into a JSON-safe form by replacing characters that
 * are illegal inside a JSON string value.
 *
 * The input is iterated as unsigned char so that bytes >= 0x80 (valid UTF-8
 * continuation / lead bytes) compare correctly in the switch and default branch
 * and are passed through unchanged.  JSON itself is UTF-8, so multi-byte
 * sequences don't need to be re-encoded.
 */
std::string Persistence::escapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size()); // pre-allocate to avoid repeated reallocations

    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break; // must escape to avoid ending the string
            case '\\': result += "\\\\"; break; // must escape to avoid starting an escape
            case '\n': result += "\\n";  break; // newline would break the JSONL line boundary
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    // C0 control characters are forbidden unescaped in JSON strings.
                    // Encode as \uXXXX (lowercase hex, zero-padded to 4 digits).
                    char buf[7]; // "\\u" + 4 hex digits + NUL = 7 bytes
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    // Printable ASCII and valid UTF-8 multi-byte sequences pass through.
                    result += static_cast<char>(c);
                }
                break;
        }
    }
    return result;
}

/**
 * Reverses escapeJson: converts the body of a JSON string (everything between
 * the surrounding quote characters) back into a plain C++ string.
 *
 * The function is a state machine with two states:
 *   - Normal: accumulate characters as-is.
 *   - After '\\': interpret the next character as an escape code.
 *
 * \uXXXX sequences are decoded to UTF-8.  Only BMP code points (U+0000–U+FFFF)
 * are produced by escapeJson, so surrogate pairs are not handled here.
 */
std::string Persistence::unescapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i; // consume the backslash; s[i] is now the escape character
            switch (s[i]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break; // valid but uncommon JSON escape
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u':
                    // \uXXXX — decode four hex digits into a Unicode code point,
                    // then encode that code point as UTF-8.
                    if (i + 4 < s.size()) {
                        unsigned int cp = 0;
                        for (int j = 1; j <= 4; ++j) {
                            unsigned char h = static_cast<unsigned char>(s[i + j]);
                            cp <<= 4;
                            if      (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            // Non-hex digits leave cp unchanged for that nibble.
                        }
                        i += 4; // advance past the four hex digit characters

                        // Encode the code point as UTF-8.
                        if (cp < 0x80) {
                            // U+0000–U+007F: single byte (identical to ASCII)
                            result += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            // U+0080–U+07FF: two-byte sequence
                            result += static_cast<char>(0xC0 | (cp >> 6));
                            result += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            // U+0800–U+FFFF: three-byte sequence (covers full BMP)
                            result += static_cast<char>(0xE0 | (cp >> 12));
                            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                default:
                    // Unknown escape sequence: pass the escaped character through.
                    result += s[i];
                    break;
            }
        } else {
            result += s[i]; // ordinary character — copy verbatim
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers — JSON deserialization
// ---------------------------------------------------------------------------

/**
 * Locates the first occurrence of  "key":"  in the JSON line, then reads
 * characters until the closing (unescaped) quote, collecting raw bytes
 * (including any backslash sequences) into a temporary buffer.  The buffer is
 * then decoded with unescapeJson so that escape sequences are resolved exactly
 * once.
 *
 * This two-step approach (collect-raw, then unescape) avoids re-scanning
 * partially decoded output and correctly handles sequences like  \\"  which
 * is an escaped backslash followed by the closing quote.
 */
std::string Persistence::extractString(const std::string& json, const std::string& key) {
    // Build the exact byte pattern we're searching for.
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return ""; // field not present in this line
    pos += searchKey.size(); // advance past the opening quote to the first value byte

    std::string raw; // raw (still-escaped) content of the string value
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            // Two-character escape sequence — copy both bytes into raw so that
            // unescapeJson can process them together.
            raw += json[pos];
            raw += json[pos + 1];
            pos += 2;
        } else if (json[pos] == '"') {
            break; // reached the unescaped closing quote — stop
        } else {
            raw += json[pos++];
        }
    }
    return unescapeJson(raw);
}

/**
 * Locates the array value of a given key (pattern  "key":[) and then iterates
 * through the array elements.  Each element must be a quoted string (as produced
 * by save() for the "observations" field).  Non-string elements (numbers,
 * booleans, nested objects) are silently skipped by advancing past unknown
 * characters.
 */
std::vector<std::string> Persistence::extractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":[";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return {}; // array field not present
    pos += searchKey.size(); // now positioned at the first character inside [...]

    std::vector<std::string> result;

    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] == '"') {
            // Start of a quoted string element — parse it the same way as extractString.
            ++pos; // skip opening quote
            std::string raw;
            while (pos < json.size()) {
                if (json[pos] == '\\' && pos + 1 < json.size()) {
                    raw += json[pos];
                    raw += json[pos + 1];
                    pos += 2;
                } else if (json[pos] == '"') {
                    break; // closing quote of this element
                } else {
                    raw += json[pos++];
                }
            }
            result.push_back(unescapeJson(raw));
            if (pos < json.size()) ++pos; // skip the closing quote before continuing
        } else {
            // Comma, whitespace, or an unexpected token — advance one byte.
            ++pos;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

/**
 * Writes all entities first, then all relations, each as a single JSON object
 * on its own line.  The file is opened with std::ios::trunc so any previous
 * content is discarded — the output is always a complete, self-consistent
 * snapshot of the graph at the moment save() is called.
 */
void Persistence::save(const Graph& graph) {
    std::ofstream file(m_filePath, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + m_filePath);
    }

    // --- Entities ---
    for (const auto& [name, entity] : graph.entities) {
        file << "{\"type\":\"entity\",\"name\":\""
             << escapeJson(entity.name)
             << "\",\"entityType\":\""
             << escapeJson(entity.type)
             << "\",\"observations\":[";

        // Serialize the observations array; elements are comma-separated.
        for (size_t i = 0; i < entity.observations.size(); ++i) {
            if (i > 0) file << ",";
            file << "\"" << escapeJson(entity.observations[i]) << "\"";
        }
        file << "]}\n"; // close observations array, object, and the JSONL line
    }

    // --- Relations ---
    for (const auto& rel : graph.relations) {
        file << "{\"type\":\"relation\",\"from\":\""
             << escapeJson(rel.from)
             << "\",\"relationType\":\""
             << escapeJson(rel.relationType)
             << "\",\"to\":\""
             << escapeJson(rel.to)
             << "\"}\n";
    }
}

/**
 * Reads the JSONL file line by line.  The "type" discriminator field on each
 * line determines how the rest of the object is parsed.  Lines with an unknown
 * type, missing required fields, or an unrecognizable format are silently
 * ignored so that a partially corrupted file still loads whatever records are
 * intact.
 */
Graph Persistence::load() {
    Graph graph;
    std::ifstream file(m_filePath);
    if (!file.is_open()) return graph; // normal on first run — no file yet

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue; // skip blank separator lines

        std::string type = extractString(line, "type");

        if (type == "entity") {
            Entity entity;
            entity.name         = extractString(line, "name");
            entity.type         = extractString(line, "entityType");
            entity.observations = extractArray(line, "observations");

            // Guard against loading a malformed line with no name.
            if (!entity.name.empty()) {
                graph.entities[entity.name] = std::move(entity);
            }

        } else if (type == "relation") {
            Relation rel;
            rel.from         = extractString(line, "from");
            rel.relationType = extractString(line, "relationType");
            rel.to           = extractString(line, "to");

            // Both endpoints must be present for the relation to be valid.
            if (!rel.from.empty() && !rel.to.empty()) {
                graph.relations.push_back(std::move(rel));
            }
        }
        // Lines with an unknown "type" value are intentionally ignored.
    }
    return graph;
}
