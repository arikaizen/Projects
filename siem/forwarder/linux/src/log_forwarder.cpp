/**
 * @file log_forwarder.cpp
 * @brief Implementation of LogForwarder class (Linux)
 *
 * Provides TCP socket communication functionality for forwarding logs to SIEM server.
 */

#include "log_forwarder.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

LogForwarder::LogForwarder(const std::string& server, int port)
    : serverAddress(server), serverPort(port), connected(false), sock(-1) {
}

LogForwarder::~LogForwarder() {
    disconnect();
}

bool LogForwarder::initialize() {
    std::cout << "[LogForwarder] Network initialized" << std::endl;
    return true;
}

bool LogForwarder::connect() {
    struct addrinfo hints, *result, *rp;

    // Configure connection hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve server address
    std::string portStr = std::to_string(serverPort);
    int status = getaddrinfo(serverAddress.c_str(), portStr.c_str(), &hints, &result);
    if (status != 0) {
        std::cerr << "[LogForwarder] getaddrinfo failed: " << gai_strerror(status) << std::endl;
        return false;
    }

    // Attempt to connect to the server
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // Create socket
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) {
            std::cerr << "[LogForwarder] socket() failed: " << strerror(errno) << std::endl;
            continue;
        }

        // Attempt connection
        if (::connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;  // Successfully connected
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);

    if (sock == -1) {
        std::cerr << "[LogForwarder] Unable to connect to SIEM server at "
                  << serverAddress << ":" << serverPort << std::endl;
        return false;
    }

    connected = true;
    std::cout << "[LogForwarder] Connected to SIEM server at "
              << serverAddress << ":" << serverPort << std::endl;
    return true;
}

void LogForwarder::disconnect() {
    if (sock != -1) {
        close(sock);
        sock = -1;
        std::cout << "[LogForwarder] Disconnected from SIEM server" << std::endl;
    }
    connected = false;
}

bool LogForwarder::sendLog(const std::string& logData) {
    if (!connected || sock == -1) {
        std::cerr << "[LogForwarder] Cannot send: Not connected to server" << std::endl;
        return false;
    }

    // Append newline delimiter for JSON streaming
    std::string message = logData + "\n";

    // Send data over TCP socket
    ssize_t result = send(sock, message.c_str(), message.length(), 0);
    if (result == -1) {
        std::cerr << "[LogForwarder] send() failed: " << strerror(errno) << std::endl;
        connected = false;
        return false;
    }

    return true;
}

bool LogForwarder::isConnected() const {
    return connected;
}
