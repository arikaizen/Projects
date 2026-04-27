/**
 * @file db_client.cpp
 * @brief TCP client implementation and tool-to-command translation.
 */

#include "db_client.h"

#include <sys/socket.h>   // socket(), connect(), recv(), ::send(), shutdown()
#include <netinet/in.h>   // sockaddr_in
#include <netdb.h>        // getaddrinfo(), freeaddrinfo()
#include <unistd.h>       // close()

#include <cstring>        // std::memset
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DbClient::DbClient(const std::string& host, int port)
    : m_host(host), m_port(port), m_fd(-1) {}

DbClient::~DbClient() {
    disconnect();
}

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------

bool DbClient::connect() {
    // Use getaddrinfo so both "127.0.0.1" (IP) and "localhost" (hostname) work.
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;       // IPv4 only
    hints.ai_socktype = SOCK_STREAM;   // TCP

    struct addrinfo* res = nullptr;
    if (getaddrinfo(m_host.c_str(),
                    std::to_string(m_port).c_str(),
                    &hints, &res) != 0 || res == nullptr) {
        return false; // host not resolvable
    }

    // Create the socket.
    m_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_fd < 0) {
        freeaddrinfo(res);
        return false;
    }

    // Attempt the TCP three-way handshake.
    if (::connect(m_fd, res->ai_addr,
                  static_cast<socklen_t>(res->ai_addrlen)) < 0) {
        close(m_fd);
        m_fd = -1;
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);
    m_recvBuffer.clear();
    return true;
}

// ---------------------------------------------------------------------------
// send
// ---------------------------------------------------------------------------

std::string DbClient::send(const std::string& command) {
    if (m_fd < 0) return "ERROR not connected";

    // Append newline to form a complete graph DB protocol line.
    std::string line = command + "\n";
    ssize_t sent = ::send(m_fd, line.c_str(), line.size(), 0);
    if (sent < 0) return "ERROR send failed";

    // Read bytes from the socket into m_recvBuffer until a '\n' is found.
    // We may need multiple recv() calls if the response is fragmented.
    while (true) {
        size_t nl = m_recvBuffer.find('\n');
        if (nl != std::string::npos) {
            // Complete line found — extract it and trim the trailing '\r' if present.
            std::string response = m_recvBuffer.substr(0, nl);
            m_recvBuffer.erase(0, nl + 1); // consume through '\n'
            if (!response.empty() && response.back() == '\r')
                response.pop_back();
            return response;
        }

        // No '\n' yet — read more data from the socket.
        char buf[4096];
        ssize_t n = recv(m_fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            // recv() returning 0 means the server closed the connection.
            // recv() returning < 0 means a socket error.
            disconnect();
            return "ERROR connection lost";
        }
        m_recvBuffer.append(buf, static_cast<size_t>(n));
    }
}

// ---------------------------------------------------------------------------
// disconnect
// ---------------------------------------------------------------------------

void DbClient::disconnect() {
    if (m_fd >= 0) {
        // Gracefully shut down both halves before closing so the server's
        // select() loop sees a clean EOF rather than a RST.
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
        m_fd = -1;
    }
    m_recvBuffer.clear();
}

// ---------------------------------------------------------------------------
// isConnected
// ---------------------------------------------------------------------------

bool DbClient::isConnected() const {
    return m_fd >= 0;
}

// ---------------------------------------------------------------------------
// translateToDbCommand
// ---------------------------------------------------------------------------

/**
 * Maps a tool name + argument map onto the graph DB wire command.
 *
 * The graph DB uses a positional " | " delimiter (space-pipe-space) for multi-
 * argument commands.  The argument values come from the ToolCall.arguments map
 * already decoded — no JSON escaping is applied here because the graph DB
 * protocol does not use JSON.
 */
std::string DbClient::translateToDbCommand(const ToolCall& call) {
    // Helper: look up an argument value by name; returns "" if absent.
    auto get = [&](const std::string& key) -> const std::string& {
        static const std::string empty;
        auto it = call.arguments.find(key);
        return it != call.arguments.end() ? it->second : empty;
    };

    if (call.name == "create_entity")
        return "CREATE_ENTITY " + get("name") + " | " + get("type");

    if (call.name == "add_observation")
        return "ADD_OBS " + get("entity") + " | " + get("observation");

    if (call.name == "create_relation")
        return "CREATE_REL " + get("from") + " | " + get("relationType") + " | " + get("to");

    if (call.name == "delete_entity")
        return "DELETE_ENTITY " + get("name");

    if (call.name == "delete_observation")
        return "DELETE_OBS " + get("entity") + " | " + get("observation");

    if (call.name == "delete_relation")
        return "DELETE_REL " + get("from") + " | " + get("relationType") + " | " + get("to");

    if (call.name == "search_nodes")
        return "SEARCH " + get("query");

    if (call.name == "get_relations")
        return "GET_RELATIONS " + get("entity");

    if (call.name == "read_graph")
        return "READ_GRAPH";

    return ""; // unknown tool — caller checks for empty string
}
