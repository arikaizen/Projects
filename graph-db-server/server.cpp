#include "server.h"
#include "request_handler.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>

#include <map>
#include <string>
#include <stdexcept>
#include <iostream>

Server::Server(int port, RequestHandler& handler)
    : m_port(port), m_handler(handler), m_listenFd(-1) {}

void Server::run() {
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(m_listenFd);
        throw std::runtime_error("Failed to bind to port " + std::to_string(m_port));
    }

    if (listen(m_listenFd, SOMAXCONN) < 0) {
        close(m_listenFd);
        throw std::runtime_error("Failed to listen on socket");
    }

    fd_set masterSet;
    FD_ZERO(&masterSet);
    FD_SET(m_listenFd, &masterSet);
    int maxFd = m_listenFd;

    std::map<int, std::string> clientBuffers;

    while (true) {
        fd_set readSet = masterSet;
        int ready = select(maxFd + 1, &readSet, nullptr, nullptr, nullptr);
        if (ready < 0) {
            std::cerr << "select() error\n";
            break;
        }

        for (int fd = 0; fd <= maxFd; ++fd) {
            if (!FD_ISSET(fd, &readSet)) continue;

            if (fd == m_listenFd) {
                int clientFd = accept(m_listenFd, nullptr, nullptr);
                if (clientFd < 0) continue;
                FD_SET(clientFd, &masterSet);
                if (clientFd > maxFd) maxFd = clientFd;
                clientBuffers[clientFd] = "";
                std::cout << "Client connected: fd=" << clientFd << "\n";
            } else {
                char buf[4096];
                ssize_t n = recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    std::cout << "Client disconnected: fd=" << fd << "\n";
                    close(fd);
                    FD_CLR(fd, &masterSet);
                    clientBuffers.erase(fd);
                    if (fd == maxFd) {
                        while (maxFd > m_listenFd && !FD_ISSET(maxFd, &masterSet))
                            --maxFd;
                    }
                } else {
                    clientBuffers[fd].append(buf, static_cast<size_t>(n));
                    std::string& buffer = clientBuffers[fd];
                    size_t pos;
                    while ((pos = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, pos);
                        buffer.erase(0, pos + 1);
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        std::string response = m_handler.handle(line);
                        send(fd, response.c_str(), response.size(), 0);
                    }
                }
            }
        }
    }

    close(m_listenFd);
}
