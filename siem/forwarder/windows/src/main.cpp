/**
 * @file main.cpp
 * @brief Main entry point for Windows Event Log Forwarder
 *
 * This is the minimal main function that parses command-line arguments
 * and delegates execution to the Forwarder API.
 *
 * Usage:
 *   log_forwarder.exe [server_address] [port]
 *
 * Arguments:
 *   server_address - SIEM server IP or hostname (default: 127.0.0.1)
 *   port          - SIEM server port number (default: 8089)
 *
 * Examples:
 *   log_forwarder.exe
 *   log_forwarder.exe 192.168.1.100
 *   log_forwarder.exe 192.168.1.100 8089
 */

#include "forwarder_api.h"
#include <cstdlib>

/**
 * @brief Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, non-zero on error
 */
int main(int argc, char* argv[]) {
    // Default configuration
    std::string serverAddress = DEFAULT_SIEM_SERVER;
    int serverPort = DEFAULT_SIEM_PORT;

    // Parse command-line arguments
    if (argc >= 2) {
        serverAddress = argv[1];
    }
    if (argc >= 3) {
        serverPort = std::atoi(argv[2]);
    }

    // Run the forwarder (this call blocks indefinitely)
    return runForwarder(serverAddress, serverPort);
}
