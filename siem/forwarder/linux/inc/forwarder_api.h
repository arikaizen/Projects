/**
 * @file forwarder_api.h
 * @brief Main Linux System Log Forwarder API
 *
 * This is the primary API for the Linux System Log Forwarder system.
 * It provides high-level functions to initialize and run the log forwarding service.
 */

#ifndef FORWARDER_API_H
#define FORWARDER_API_H

#include "log_forwarder.h"
#include <string>

// Default configuration constants
const char* const DEFAULT_SIEM_SERVER = "127.0.0.1";  ///< Default SIEM server address
const int DEFAULT_SIEM_PORT = 8089;                   ///< Default SIEM server port
const int RECONNECT_DELAY_MS = 5000;                  ///< Reconnection delay in milliseconds

/**
 * @brief Monitor and forward Linux system logs from journald
 *
 * Subscribes to systemd journal and continuously monitors for new log entries.
 * Entries are formatted as JSON and forwarded to the SIEM server via the provided
 * LogForwarder instance. Automatically handles reconnection on network failures.
 *
 * @param forwarder Reference to initialized LogForwarder instance
 *
 * @note This function runs in an infinite loop and will block until terminated
 */
void forwardSystemLogs(LogForwarder& forwarder);

/**
 * @brief Initialize and run the Linux System Log Forwarder
 *
 * This is the main entry point for the forwarder API. It initializes the network
 * connection, establishes connection to the SIEM server, and begins monitoring
 * system logs via journald.
 *
 * @param serverAddress SIEM server IP address or hostname
 * @param serverPort SIEM server port number
 * @return 0 on successful completion, non-zero on error
 *
 * @note This function will block indefinitely while monitoring logs
 */
int runForwarder(const std::string& serverAddress, int serverPort);

#endif // FORWARDER_API_H
