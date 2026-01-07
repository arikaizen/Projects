/**
 * @file log_forwarder.cpp
 * @brief Implementation of LogForwarder class
 *
 * Provides TCP socket communication functionality for forwarding logs to SIEM server.
 */

#include "log_forwarder.h"
#include "logger.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

LogForwarder::LogForwarder(const std::string& server, int port)
    : serverAddress(server), serverPort(port), connected(false), sock(INVALID_SOCKET) {
}

LogForwarder::~LogForwarder() {
    disconnect();
}

bool LogForwarder::initialize() {
    WSADATA wsaData;
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
    struct addrinfo *result = NULL, *ptr = NULL, hints;

    // Configure connection hints
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve server address
    std::string portStr = std::to_string(serverPort);
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
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Create socket
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            std::cerr << "[LogForwarder] socket() failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Attempt connection
        iResult = ::connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;  // Successfully connected
    }

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
        closesocket(sock);
        sock = INVALID_SOCKET;
        std::cout << "[LogForwarder] Disconnected from SIEM server" << std::endl;
        if (g_logger) {
            g_logger->info("LogForwarder", "Disconnected from SIEM server", "");
        }
    }
    connected = false;
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
    int result = send(sock, message.c_str(), (int)message.length(), 0);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cerr << "[LogForwarder] send() failed with error: " << error << std::endl;
        if (g_logger) {
            g_logger->error("LogForwarder", "Failed to send log",
                          "Error code: " + std::to_string(error));
        }
        connected = false;
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
