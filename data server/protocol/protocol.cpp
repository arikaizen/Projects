/**
 * @file protocol.cpp
 * @brief Implementation of request parsing and response formatting.
 */

#include "protocol.h"

#include <cstdio> // std::snprintf

namespace protocol {

/**
 * Parsing algorithm:
 *   1. Trim surrounding whitespace so that telnet CRLF endings and extra
 *      spaces in interactive sessions don't cause "Unknown command" errors.
 *   2. Split at the first space to separate command from argument string.
 *   3. Split the argument string on the three-character delimiter " | "
 *      (space-pipe-space) to produce individual arguments.
 *
 * The " | " delimiter was chosen (with mandatory surrounding spaces) so that
 * argument values themselves can freely contain spaces without needing quoting.
 * The trade-off is that values cannot contain the literal sequence " | ".
 */
Request parseRequest(const std::string& raw) {
    Request req;

    // --- Step 1: trim whitespace ---
    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return req; // blank line — return empty Request

    size_t end = raw.find_last_not_of(" \t\r\n");
    std::string trimmed = raw.substr(start, end - start + 1);

    // --- Step 2: split command from argument string ---
    size_t spacePos = trimmed.find(' ');
    if (spacePos == std::string::npos) {
        // No space found — the entire trimmed string is the command (e.g. "READ_GRAPH").
        req.command = trimmed;
        return req;
    }

    req.command = trimmed.substr(0, spacePos);
    std::string rest = trimmed.substr(spacePos + 1); // everything after the first space

    // --- Step 3: split argument string on " | " ---
    size_t pos = 0;
    while (true) {
        size_t sep = rest.find(" | ", pos);
        if (sep == std::string::npos) {
            // No more separators — the remainder is the last (or only) argument.
            req.args.push_back(rest.substr(pos));
            break;
        }
        req.args.push_back(rest.substr(pos, sep - pos)); // argument before separator
        pos = sep + 3; // advance past the three-character " | " separator
    }
    return req;
}

std::string formatOK(const std::string& json) {
    // The trailing '\n' is the line delimiter expected by the server's recv loop.
    return "OK " + json + "\n";
}

std::string formatError(const std::string& message) {
    return "ERROR " + message + "\n";
}

/**
 * Iterates the input as unsigned char so that bytes >= 0x80 compare
 * correctly in the switch/default — they are ordinary bytes of a
 * multi-byte UTF-8 sequence and must pass through unchanged.
 */
std::string jsonEscape(const std::string& s) {
    std::string result;
    result.reserve(s.size());

    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break; // unescaped quote would terminate JSON string
            case '\\': result += "\\\\"; break; // unescaped backslash would start an escape
            case '\n': result += "\\n";  break; // newline breaks JSONL line boundary
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control characters below SPACE are not allowed bare in JSON.
                    char buf[7]; // fits "\\u" + 4 hex digits + NUL
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c); // ASCII printable or UTF-8 byte
                }
                break;
        }
    }
    return result;
}

} // namespace protocol
