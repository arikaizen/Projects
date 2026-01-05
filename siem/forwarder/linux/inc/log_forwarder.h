/**
 * @file log_forwarder.h
 * @brief Network log forwarder for SIEM integration (Linux)
 *
 * This module provides TCP socket communication functionality to forward
 * Linux system logs to a remote SIEM server.
 */

#ifndef LOG_FORWARDER_H
#define LOG_FORWARDER_H

#include <string>

/**
 * @class LogForwarder
 * @brief Handles TCP connection and log transmission to SIEM server
 *
 * This class manages the network connection to the SIEM server and provides
 * methods to send log data over TCP. It handles connection lifecycle including
 * initialization, connection, disconnection, and automatic reconnection.
 */
class LogForwarder {
private:
    int sock;                       ///< TCP socket file descriptor
    bool connected;                 ///< Connection status flag
    std::string serverAddress;      ///< SIEM server IP address or hostname
    int serverPort;                 ///< SIEM server port number

public:
    /**
     * @brief Construct a new Log Forwarder object
     * @param server SIEM server address (IP or hostname)
     * @param port SIEM server port number
     */
    LogForwarder(const std::string& server, int port);

    /**
     * @brief Destroy the Log Forwarder object and cleanup resources
     */
    ~LogForwarder();

    /**
     * @brief Initialize network connection
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Establish TCP connection to SIEM server
     * @return true if connection successful, false otherwise
     */
    bool connect();

    /**
     * @brief Close connection and cleanup socket resources
     */
    void disconnect();

    /**
     * @brief Send log data to SIEM server
     * @param logData JSON-formatted log string to send
     * @return true if send successful, false otherwise
     */
    bool sendLog(const std::string& logData);

    /**
     * @brief Check if currently connected to SIEM server
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
};

#endif // LOG_FORWARDER_H
