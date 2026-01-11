/**
 * @file log_forwarder.cpp
 * @brief Implementation of LogForwarder class
 *
 * Provides TCP socket communication functionality for forwarding logs to SIEM server.
 */

#include "log_forwarder.h" // socket lifecycle APIs
#include "logger.h"        // g_logger helpers
#include <iostream>         // std::cout, std::cerr

// ws2_32.lib: WSAStartup, getaddrinfo, socket, connect, send, closesocket,
//              WSACleanup, WSAGetLastError, freeaddrinfo
#pragma comment(lib, "ws2_32.lib")

LogForwarder::LogForwarder(const std::string& server, int port)
    : serverAddress(server), serverPort(port), connected(false), sock(INVALID_SOCKET) {
}

LogForwarder::~LogForwarder() {
    disconnect();
}

bool LogForwarder::initialize() {
    WSADATA wsaData;
    // WSAStartup: Initialize Windows Sockets library (version 2.2). Returns 0 on success, nonzero error code on failure.
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[LogForwarder] WSAStartup failed with error: " << result << std::endl;
        if (g_logger) {
            g_logger->error("LogForwarder", "WSAStartup failed",
                          "Error code: " + std::to_string(result));
        }
        return false;
    }
    std::cout << "[LogForwarder] Windows Sockets initialized successfully" << std::endl;
    if (g_logger) {
        g_logger->info("LogForwarder", "Windows Sockets initialized successfully", "");
    }
    return true;
}

bool LogForwarder::connect() {
    struct addrinfo *result = nullptr, *ptr = nullptr, hints;

    // Configure connection hints
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve server address
    std::string portStr = std::to_string(serverPort);
    // getaddrinfo: Convert hostname/IP and port to address info structure. Returns 0 on success, nonzero error code on failure.
    int iResult = getaddrinfo(serverAddress.c_str(), portStr.c_str(), &hints, &result);
    if (iResult != 0) {
        std::cerr << "[LogForwarder] getaddrinfo failed with error: " << iResult << std::endl;
        if (g_logger) {
            g_logger->error("LogForwarder", "Failed to resolve server address",
                          serverAddress + ":" + portStr + " Error: " + std::to_string(iResult));
        }
        return false;
    }

    // Attempt to connect to the server
    sock = INVALID_SOCKET;
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        // Create socket
        // socket: Create a socket for communication. Returns socket handle on success, INVALID_SOCKET on failure.
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            std::cerr << "[LogForwarder] socket() failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Attempt connection
        // connect: Establish TCP connection to remote server. Returns 0 on success, SOCKET_ERROR on failure.
        iResult = ::connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            // closesocket: Close socket and release associated resources. Returns 0 on success, SOCKET_ERROR on failure.
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;  // Successfully connected
    }

    // freeaddrinfo: Free allocated address info structure. Returns nothing (void).
    freeaddrinfo(result);

    if (sock == INVALID_SOCKET) {
        std::cerr << "[LogForwarder] Unable to connect to SIEM server at "
                  << serverAddress << ":" << serverPort << std::endl;
        if (g_logger) {
            g_logger->error("LogForwarder", "Unable to connect to SIEM server",
                          serverAddress + ":" + std::to_string(serverPort));
        }
        return false;
    }

    connected = true;
    std::cout << "[LogForwarder] Connected to SIEM server at "
              << serverAddress << ":" << serverPort << std::endl;
    if (g_logger) {
        g_logger->info("LogForwarder", "Connected to SIEM server",
                      serverAddress + ":" + std::to_string(serverPort));
    }
    return true;
}

void LogForwarder::disconnect() {
    if (sock != INVALID_SOCKET) {
        // closesocket: Close socket and release associated resources. Returns 0 on success, SOCKET_ERROR on failure.
        closesocket(sock);
        sock = INVALID_SOCKET;
        std::cout << "[LogForwarder] Disconnected from SIEM server" << std::endl;
        if (g_logger) {
            g_logger->info("LogForwarder", "Disconnected from SIEM server", "");
        }
    }
    connected = false;
    // WSACleanup: Uninitialize Windows Sockets library and release resources. Returns 0 on success, nonzero error code on failure.
    WSACleanup();
}

bool LogForwarder::sendLog(const std::string& logData) {
    if (!connected || sock == INVALID_SOCKET) {
        std::cerr << "[LogForwarder] Cannot send: Not connected to server" << std::endl;
        if (g_logger) {
            g_logger->warning("LogForwarder", "Cannot send log - not connected", "");
        }
        return false;
    }

    // Append newline delimiter for JSON streaming
    std::string message = logData + "\n";

    // Send data over TCP socket
    // send: Transmit data over TCP socket. Returns number of bytes sent on success, SOCKET_ERROR on failure.
    int result = send(sock, message.c_str(), (int)message.length(), 0);
    if (result == SOCKET_ERROR) {
        // WSAGetLastError: Retrieve error code from last failed Winsock operation. Returns platform-specific error code (WSAECONNRESET, WSAENOTCONN, etc).
        int error = WSAGetLastError();
        std::cerr << "[LogForwarder] send() failed with error: " << error << std::endl;
        if (g_logger) {
            g_logger->error("LogForwarder", "Failed to send log",
                          "Error code: " + std::to_string(error));
        }
        connected = false;
        return false;
    }
    
    // Verify all bytes were sent
    if (result != (int)message.length()) {
        std::cerr << "[LogForwarder] Partial send: " << result << "/" << message.length() << " bytes" << std::endl;
        if (g_logger) {
            g_logger->warning("LogForwarder", "Partial send detected",
                            "Sent: " + std::to_string(result) + "/" + std::to_string(message.length()) + " bytes");
        }
        // Note: In production, you may want to retry sending remaining bytes
        return false;
    }

    if (g_logger) {
        // Log successful sends with truncated data for readability
        std::string truncated = logData.substr(0, 100);
        if (logData.length() > 100) truncated += "...";
        g_logger->debug("LogForwarder", "Log sent successfully",
                       "Bytes: " + std::to_string(result) + " Data: " + truncated);
    }

    return true;
}

bool LogForwarder::isConnected() const {
    return connected;
}
