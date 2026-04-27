/**
 * @file server.cpp
 * @brief Implementation of the single-threaded select()-based TCP server.
 */

#include "server.h"
#include "request_handler.h"  // full definition needed to call handle()

#include <sys/socket.h>   // socket(), bind(), listen(), accept(), recv(), send(), setsockopt()
#include <sys/select.h>   // select(), fd_set, FD_ZERO, FD_SET, FD_CLR, FD_ISSET
#include <netinet/in.h>   // sockaddr_in, INADDR_ANY, htons()
#include <unistd.h>       // close()

#include <map>            // per-client read buffers
#include <string>
#include <stdexcept>
#include <iostream>       // connection log messages to stdout/stderr

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Server::Server(int port, RequestHandler& handler)
    : m_port(port), m_handler(handler), m_listenFd(-1) {}

// ---------------------------------------------------------------------------
// run() — the blocking event loop
// ---------------------------------------------------------------------------

void Server::run() {

    // -----------------------------------------------------------------------
    // 1. Create a TCP socket.
    //    AF_INET      = IPv4
    //    SOCK_STREAM  = reliable byte-stream (TCP)
    //    0            = let the OS choose the protocol (TCP for SOCK_STREAM)
    // -----------------------------------------------------------------------
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // -----------------------------------------------------------------------
    // 2. Enable SO_REUSEADDR.
    //    Without this option, if the server is restarted within the TCP
    //    TIME_WAIT window (~60 s after the previous run), bind() would fail
    //    with EADDRINUSE.  SO_REUSEADDR allows the port to be reused
    //    immediately, which is essential during development.
    // -----------------------------------------------------------------------
    int opt = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // -----------------------------------------------------------------------
    // 3. Bind to all network interfaces on the chosen port.
    //    Zero-initialising addr{} ensures sin_zero padding is zeroed, which
    //    is required on some platforms.
    // -----------------------------------------------------------------------
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;               // accept connections on all interfaces
    addr.sin_port        = htons(static_cast<uint16_t>(m_port)); // convert to network byte order

    if (bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(m_listenFd);
        throw std::runtime_error("Failed to bind to port " + std::to_string(m_port));
    }

    // -----------------------------------------------------------------------
    // 4. Start listening.
    //    SOMAXCONN is the system-defined maximum backlog for pending connections
    //    (typically 128 on Linux).  Connections beyond this limit are dropped
    //    by the kernel until accept() is called.
    // -----------------------------------------------------------------------
    if (listen(m_listenFd, SOMAXCONN) < 0) {
        close(m_listenFd);
        throw std::runtime_error("Failed to listen on socket");
    }

    // -----------------------------------------------------------------------
    // 5. Set up the master fd_set and tracking state.
    //
    //    masterSet  — the authoritative set of all fds we care about.  It is
    //                 copied into readSet before each select() call because
    //                 select() modifies its fd_set argument in place.
    //    maxFd      — the highest-numbered fd currently in masterSet.  select()
    //                 requires nfds = maxFd + 1 so it knows how many bits to
    //                 inspect.  We track it explicitly and shrink it when the
    //                 highest-numbered client disconnects.
    //    clientBuffers — maps each connected client fd to its incomplete line
    //                 buffer.  TCP is a stream protocol; a single recv() may
    //                 deliver half a line, so we accumulate bytes here until
    //                 a '\n' is found.
    // -----------------------------------------------------------------------
    fd_set masterSet;
    FD_ZERO(&masterSet);
    FD_SET(m_listenFd, &masterSet);
    int maxFd = m_listenFd;

    std::map<int, std::string> clientBuffers; // fd → accumulated partial line

    // -----------------------------------------------------------------------
    // 6. Event loop — runs indefinitely until select() fails.
    // -----------------------------------------------------------------------
    while (true) {

        // Copy masterSet because select() overwrites the fd_set with only the
        // ready fds; we need masterSet untouched for the next iteration.
        fd_set readSet = masterSet;

        // Block until at least one fd in readSet is readable.
        // Passing nullptr for the timeout means "wait forever".
        int ready = select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
        if (ready < 0) {
            std::cerr << "select() error\n";
            break; // unrecoverable — exit the loop and close the server
        }

        // Iterate every fd from 0 to maxFd; skip those not in readSet.
        for (int fd = 0; fd <= maxFd; ++fd) {
            if (!FD_ISSET(fd, &readSet)) continue;

            // -----------------------------------------------------------
            // Case A: the listening socket is readable → new connection.
            // -----------------------------------------------------------
            if (fd == m_listenFd) {
                // accept() completes the TCP three-way handshake and returns a
                // fresh socket for communication with the new client.
                // nullptr, nullptr — we don't need the client's address.
                int clientFd = accept(m_listenFd, nullptr, nullptr);
                if (clientFd < 0) continue; // accept failed (e.g. EINTR) — skip

                FD_SET(clientFd, &masterSet);
                if (clientFd > maxFd) maxFd = clientFd; // update the upper bound
                clientBuffers[clientFd] = "";             // allocate an empty buffer

                std::cout << "Client connected: fd=" << clientFd << "\n";

            // -----------------------------------------------------------
            // Case B: a client socket is readable → data arrived or EOF.
            // -----------------------------------------------------------
            } else {
                char buf[4096];
                // recv() returns:
                //   > 0  bytes actually received
                //   == 0  the client closed the connection gracefully (EOF)
                //   < 0  an error occurred
                ssize_t n = recv(fd, buf, sizeof(buf), 0);

                if (n <= 0) {
                    // Client disconnected or recv error — clean up.
                    std::cout << "Client disconnected: fd=" << fd << "\n";
                    close(fd);
                    FD_CLR(fd, &masterSet);
                    clientBuffers.erase(fd);

                    // Shrink maxFd downward if this was the highest-numbered fd.
                    // This keeps select()'s nfds argument as small as possible.
                    if (fd == maxFd) {
                        while (maxFd > m_listenFd && !FD_ISSET(maxFd, &masterSet))
                            --maxFd;
                    }

                } else {
                    // Append newly received bytes to the client's partial buffer.
                    clientBuffers[fd].append(buf, static_cast<size_t>(n));
                    std::string& buffer = clientBuffers[fd];

                    // Extract and dispatch every complete line in the buffer.
                    // A single recv() may contain zero, one, or many '\n' chars.
                    size_t pos;
                    while ((pos = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, pos); // text before '\n'
                        buffer.erase(0, pos + 1);                  // consume through '\n'

                        // Strip a trailing '\r' to handle Windows-style CRLF endings
                        // sent by telnet or other clients that use "\r\n".
                        if (!line.empty() && line.back() == '\r') line.pop_back();

                        // Dispatch the complete line to the request handler and
                        // send the response back on the same socket.
                        std::string response = m_handler.handle(line);
                        send(fd, response.c_str(), response.size(), 0);
                    }
                    // Any bytes remaining in buffer after the last '\n' are kept
                    // for the next recv() call — they form the start of the next line.
                }
            }
        }
    }

    close(m_listenFd);
}
