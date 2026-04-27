/**
 * @file protocol.h
 * @brief Text-based line protocol for client–server communication.
 *
 * Wire format (client → server)
 * -----------------------------
 * Every request is a single UTF-8 line terminated by '\n' (LF) or "\r\n"
 * (CRLF for telnet compatibility).  The format is:
 *
 *   COMMAND arg1 | arg2 | arg3\n
 *
 * The command is a single uppercase token.  Arguments are separated by the
 * three-character sequence " | " (space-pipe-space) so that individual
 * arguments may contain spaces.
 *
 * Supported commands and their arguments:
 *   CREATE_ENTITY   name | type
 *   ADD_OBS         entity | observation
 *   CREATE_REL      from | relationType | to
 *   DELETE_ENTITY   name
 *   DELETE_OBS      entity | observation
 *   DELETE_REL      from | relationType | to
 *   SEARCH          query
 *   GET_RELATIONS   entity
 *   READ_GRAPH      (no arguments)
 *
 * Wire format (server → client)
 * -----------------------------
 * Every response is a single line terminated by '\n'.  Successful responses
 * carry a JSON payload; error responses carry a human-readable message:
 *
 *   OK {"entities":[...],"relations":[...]}
 *   ERROR Entity not found: Alice
 *
 * Mutation commands (CREATE_*, ADD_*, DELETE_*) return  OK {}  on success.
 * Query commands return  OK <json-array-or-object>.
 *
 * JSON escaping
 * -------------
 * jsonEscape() is shared between this module (for building responses) and
 * Persistence (which has its own copy to avoid a dependency on protocol).
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>

namespace protocol {

/**
 * @brief A parsed client request broken into its command token and argument list.
 *
 * After parseRequest() returns, command contains the uppercase verb (e.g.
 * "CREATE_ENTITY") and args contains each space-pipe-space-delimited argument
 * in order.  Both are empty strings / an empty vector for a blank input line.
 */
struct Request {
    /** The uppercase command token extracted from the raw request line. */
    std::string command;

    /**
     * Ordered list of arguments split on " | ".
     * args[0] is the first argument, args[1] the second, and so on.
     */
    std::vector<std::string> args;
};

/**
 * @brief Parses a raw request line into a Request struct.
 *
 * Leading and trailing whitespace (spaces, tabs, CR, LF) is trimmed from the
 * input before parsing.  The first space-separated token becomes the command;
 * everything after the first space is split on " | " to produce the args list.
 *
 * A line with no space (e.g. "READ_GRAPH") results in an empty args vector.
 * A blank or whitespace-only line results in an empty command and empty args.
 *
 * @param raw  The raw text received from the client, including any line ending.
 * @return     A populated Request; never throws.
 */
Request parseRequest(const std::string& raw);

/**
 * @brief Wraps a JSON payload in a successful response line.
 *
 * @param json  A valid JSON value string (object, array, or "{}").
 * @return      "OK <json>\n"
 */
std::string formatOK(const std::string& json);

/**
 * @brief Wraps a message in an error response line.
 *
 * @param message  Human-readable description of what went wrong.
 * @return         "ERROR <message>\n"
 */
std::string formatError(const std::string& message);

/**
 * @brief Escapes a C++ string for safe embedding in a JSON string value.
 *
 * Replaces characters that are illegal inside JSON strings with their standard
 * escape sequences.  The returned value does NOT include the surrounding quote
 * characters — the caller is responsible for adding those.
 *
 * Characters handled:
 *   '"'  → \"     '\\' → \\     '\n' → \n
 *   '\r' → \r     '\t' → \t
 *   bytes < 0x20  → \uXXXX
 *   all others    → unchanged (UTF-8 bytes pass through)
 *
 * @param s  The raw string to escape.
 * @return   The JSON-safe escaped form of s.
 */
std::string jsonEscape(const std::string& s);

} // namespace protocol

#endif // PROTOCOL_H
