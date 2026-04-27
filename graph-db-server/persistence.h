/**
 * @file persistence.h
 * @brief Write-through JSONL persistence for the in-memory graph.
 *
 * Persistence serializes and deserializes a Graph to/from a JSON Lines (.jsonl)
 * file — one self-contained JSON object per line.  Two line shapes are used:
 *
 *   Entity line:
 *     {"type":"entity","name":"Alice","entityType":"person","observations":["Speaks Spanish"]}
 *
 *   Relation line:
 *     {"type":"relation","from":"Alice","relationType":"works_at","to":"Acme"}
 *
 * The JSONL format was chosen because:
 *   - Each line is independently parseable; a truncated file loses only the
 *     last incomplete line, not the entire database.
 *   - Appending future incremental writes is trivial (though the current
 *     implementation rewrites the whole file on every save for simplicity).
 *
 * Write-through strategy
 * ----------------------
 * GraphStore calls save() after every mutation so the on-disk file is always
 * consistent with the in-memory state.  There is no separate flush or commit
 * step required by callers.
 *
 * JSON handling
 * -------------
 * No third-party JSON library is used.  Serialization builds strings manually
 * using escapeJson().  Deserialization uses pattern-based substring extraction
 * (extractString / extractArray) which is sufficient for the tightly controlled
 * output format produced by save().
 */

#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "graph_types.h"
#include <string>
#include <vector>

/**
 * @brief Serializes and deserializes a Graph to/from a JSONL file on disk.
 *
 * Typical usage:
 * @code
 *   Persistence p("graph.jsonl");
 *   Graph g = p.load();          // restore state from disk (empty graph if file absent)
 *   // ... pass g and p to GraphStore, which calls p.save() on every mutation ...
 * @endcode
 */
class Persistence {
public:
    /**
     * @brief Constructs a Persistence instance bound to the given file path.
     *
     * The file is not opened or created at construction time; opening happens
     * lazily inside save() and load().
     *
     * @param filePath Path to the .jsonl file to read from and write to.
     *                 The file need not exist yet; load() returns an empty Graph
     *                 if it is absent, and save() creates it on first write.
     */
    explicit Persistence(const std::string& filePath);

    /**
     * @brief Writes the entire graph to disk, replacing any previous content.
     *
     * The file is opened in truncate mode so the output is always a clean
     * snapshot of the current graph state.  All entities are written first,
     * followed by all relations.
     *
     * @param graph The graph to serialize.
     * @throws std::runtime_error if the file cannot be opened for writing.
     */
    void save(const Graph& graph);

    /**
     * @brief Reads the graph from disk and returns it.
     *
     * Parses the JSONL file line by line.  Lines whose "type" field is
     * "entity" are deserialized into Entity objects; lines whose "type" is
     * "relation" are deserialized into Relation objects.  Unrecognized or
     * malformed lines are silently skipped.
     *
     * If the file does not exist, an empty Graph is returned without error —
     * this is the normal startup case for a fresh deployment.
     *
     * @return The deserialized Graph, or an empty Graph if the file is absent.
     */
    Graph load();

private:
    /** Absolute or relative path to the backing .jsonl file. */
    std::string m_filePath;

    /**
     * @brief Escapes a raw C++ string into a JSON-safe string value (without quotes).
     *
     * Characters that must be escaped inside a JSON string are replaced with
     * their two-character escape sequences.  C0 control characters below 0x20
     * that are not handled by a named escape are encoded as \\uXXXX.
     *
     * @param s The raw string to escape.
     * @return The escaped string, suitable for embedding between JSON quote characters.
     */
    static std::string escapeJson(const std::string& s);

    /**
     * @brief Converts a JSON string value (without surrounding quotes) back to a
     *        plain C++ string.
     *
     * Handles all standard JSON escape sequences: \\\", \\\\, \\/,  \\n, \\r,
     * \\t, and \\uXXXX (BMP code points encoded as UTF-8).  Unknown escape
     * sequences are passed through as the escaped character.
     *
     * @param s A JSON string body — the content between the opening and closing
     *          quote characters, still containing backslash escapes.
     * @return The decoded plain string.
     */
    static std::string unescapeJson(const std::string& s);

    /**
     * @brief Extracts the value of a JSON string field from a flat JSON object line.
     *
     * Searches for the pattern  "key":"  and collects characters up to the
     * matching closing quote, handling embedded escape sequences correctly.
     * The collected raw (still-escaped) bytes are passed through unescapeJson
     * before being returned.
     *
     * This function works reliably only on the specific JSONL format produced
     * by save(); it is not a general-purpose JSON parser.
     *
     * @param json  A single JSONL line, e.g. {"type":"entity","name":"Alice",...}
     * @param key   The field name to look up (without quotes), e.g. "name".
     * @return The decoded string value, or "" if the key is not found.
     */
    static std::string extractString(const std::string& json, const std::string& key);

    /**
     * @brief Extracts the elements of a JSON string array field from a flat JSON object line.
     *
     * Searches for the pattern  "key":[  and then parses each quoted string
     * element until the closing  ]  is reached, decoding escape sequences in
     * each element via unescapeJson.
     *
     * Only arrays of strings (as produced by save() for observations) are supported;
     * nested objects or number elements are not handled.
     *
     * @param json  A single JSONL line containing the array field.
     * @param key   The field name to look up, e.g. "observations".
     * @return A vector of decoded string elements, or an empty vector if the key
     *         is not found or the array is empty.
     */
    static std::vector<std::string> extractArray(const std::string& json, const std::string& key);
};

#endif // PERSISTENCE_H
