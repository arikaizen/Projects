/**
 * @file db_client.h
 * @brief TCP client that maintains a persistent connection to the graph DB server.
 *
 * DbClient owns a single TCP socket connected to the graph DB server at
 * construction time.  It translates ToolCall structs into the graph DB wire
 * format and sends them synchronously, then reads and returns the single-line
 * response.
 *
 * Wire protocol recap (graph DB side)
 * ------------------------------------
 * Request:  one text line ending in '\n', e.g.  CREATE_ENTITY Alice | person\n
 * Response: one text line ending in '\n', e.g.  OK {}\n  or  ERROR message\n
 *
 * The connection is kept alive for the lifetime of the MCP server (one command
 * per tool invocation).  There is no multiplexing — the MCP server's single-
 * threaded event loop ensures commands are serialized naturally.
 *
 * Translation table
 * -----------------
 *   create_entity      → CREATE_ENTITY {name} | {type}
 *   add_observation    → ADD_OBS {entity} | {observation}
 *   create_relation    → CREATE_REL {from} | {relationType} | {to}
 *   delete_entity      → DELETE_ENTITY {name}
 *   delete_observation → DELETE_OBS {entity} | {observation}
 *   delete_relation    → DELETE_REL {from} | {relationType} | {to}
 *   search_nodes       → SEARCH {query}
 *   get_relations      → GET_RELATIONS {entity}
 *   read_graph         → READ_GRAPH
 */

#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include "mcp_types.h"   // ToolCall
#include <string>

/**
 * @brief Manages a persistent TCP connection to the graph DB server and
 *        translates tool calls into graph DB commands.
 */
class DbClient {
public:
    /**
     * @brief Constructs the client with the server address.
     *
     * Does NOT open the socket.  Call connect() before any other method.
     *
     * @param host  Hostname or IPv4 address of the graph DB server.
     * @param port  TCP port the graph DB server is listening on.
     */
    DbClient(const std::string& host, int port);

    /**
     * @brief Destructor — closes the socket if still connected.
     */
    ~DbClient();

    // Non-copyable: a socket is a unique resource.
    DbClient(const DbClient&) = delete;
    DbClient& operator=(const DbClient&) = delete;

    /**
     * @brief Opens a TCP connection to the graph DB server.
     *
     * Uses getaddrinfo() so both IPv4 dotted-decimal addresses ("127.0.0.1")
     * and hostnames ("localhost") are accepted.
     *
     * @return true on success; false if the connection could not be established
     *         (graph DB server not running, wrong port, network error).
     */
    bool connect();

    /**
     * @brief Sends one command line to the graph DB and returns its response.
     *
     * Appends '\n' to command before sending.  Reads bytes from the socket
     * until '\n' is found in the receive buffer, then returns the line without
     * the trailing newline (or '\r\n').
     *
     * On send or receive failure, returns "ERROR connection lost" so the caller
     * can propagate a meaningful error response to the LLM.
     *
     * @param command  The raw graph DB command string, WITHOUT a trailing newline,
     *                 e.g. "CREATE_ENTITY Alice | person".
     * @return         The response line from the graph DB, e.g. "OK {}" or
     *                 "ERROR Entity already exists: Alice".
     */
    std::string send(const std::string& command);

    /**
     * @brief Closes the TCP socket cleanly.
     *
     * Safe to call multiple times; subsequent calls are no-ops.
     */
    void disconnect();

    /**
     * @brief Returns true if the socket is open.
     *
     * @return true if connected; false if not yet connected or disconnected.
     */
    bool isConnected() const;

    /**
     * @brief Translates a ToolCall into the graph DB wire format command.
     *
     * Maps the tool name and its arguments to the corresponding graph DB
     * command string (without a trailing newline).  Returns an empty string
     * if the tool name is not recognized.
     *
     * @param call  The validated tool invocation to translate.
     * @return      Graph DB command string, or "" if the tool is unknown.
     */
    static std::string translateToDbCommand(const ToolCall& call);

private:
    std::string m_host;     ///< Graph DB server hostname or IP address
    int m_port;             ///< Graph DB server TCP port
    int m_fd;               ///< Socket file descriptor; -1 when not connected

    /**
     * Per-connection receive buffer.  TCP is a stream protocol; a single
     * recv() call may return partial data.  Bytes that arrive after the '\n'
     * of one response (spurious in normal operation, but defensive) are
     * preserved here for the next send() call.
     */
    std::string m_recvBuffer;
};

#endif // DB_CLIENT_H
