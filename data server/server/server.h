/**
 * @file server.h
 * @brief Single-threaded, multiplexed TCP server using POSIX select().
 *
 * The server listens on a TCP port and services multiple simultaneous clients
 * inside a single thread using the select() system call.  select() blocks until
 * at least one file descriptor in a watched set becomes readable, at which point
 * the server iterates the ready set and handles each ready descriptor:
 *
 *   - The listening socket becoming readable means a new client is connecting.
 *   - A client socket becoming readable means data has arrived (or the client
 *     has disconnected).
 *
 * This approach avoids the complexity of multi-threading (no mutexes, no race
 * conditions) at the cost of not utilizing multiple CPU cores.  Because all
 * graph operations are in-memory and fast, single-threaded multiplexing is
 * appropriate for this server's workload.
 *
 * Framing
 * -------
 * The protocol is line-oriented: each complete request is one '\n'-terminated
 * line.  Because TCP is a stream protocol, a single recv() call may deliver a
 * partial line, multiple lines, or both.  The server therefore maintains a
 * per-client string buffer and only dispatches to RequestHandler when a complete
 * '\n' is found in the buffer.
 *
 * Platform
 * --------
 * Uses only POSIX interfaces: <sys/socket.h>, <netinet/in.h>, <sys/select.h>,
 * <unistd.h>.  No Boost, no libuv, no threads.
 */

#ifndef SERVER_H
#define SERVER_H

// Forward declaration keeps this header free of RequestHandler's includes.
class RequestHandler;

/**
 * @brief Listens on a TCP port and dispatches line requests to a RequestHandler.
 *
 * Typical usage:
 * @code
 *   RequestHandler handler(store);
 *   Server server(7474, handler);
 *   server.run(); // blocks forever
 * @endcode
 */
class Server {
public:
    /**
     * @brief Constructs the server but does NOT open the socket yet.
     *
     * Socket creation, binding, and listening are all deferred to run() so
     * that construction is trivially safe and testable without side effects.
     *
     * @param port     TCP port to listen on (1–65535).  Ports below 1024
     *                 require elevated privileges on most Unix systems.
     * @param handler  The request dispatcher to call for every complete line.
     *                 Must remain valid for the lifetime of this Server.
     */
    Server(int port, RequestHandler& handler);

    /**
     * @brief Opens the listening socket and enters the event loop.
     *
     * This method does not return under normal operation.  It will throw on
     * unrecoverable setup errors (socket, bind, listen failure), and will exit
     * the loop and return if select() itself reports an error.
     *
     * Steps performed inside run():
     *   1. Create a TCP socket (AF_INET / SOCK_STREAM).
     *   2. Set SO_REUSEADDR to allow immediate rebind after a restart.
     *   3. Bind to INADDR_ANY on m_port.
     *   4. Listen with SOMAXCONN backlog.
     *   5. Enter the select() event loop (see file-level documentation).
     *
     * @throws std::runtime_error on socket(), bind(), or listen() failure.
     */
    void run();

private:
    /** TCP port to listen on, set at construction. */
    int m_port;

    /** Reference to the handler that processes each complete request line. */
    RequestHandler& m_handler;

    /**
     * File descriptor of the passive (listening) socket.
     * Initialized to -1; set to a valid fd inside run().
     */
    int m_listenFd;
};

#endif // SERVER_H
