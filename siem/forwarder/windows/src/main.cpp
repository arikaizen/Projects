/**
 * @file main.cpp
 * @brief Main entry point for Windows Event Log Forwarder
 *
 * This is the minimal main function that parses command-line arguments
 * and delegates execution to the Forwarder API.
 *
 * Usage:
 *   log_forwarder.exe [server_address] [port] [mode] [hours_back]
 *
 * Arguments:
 *   server_address - SIEM server IP or hostname (default: 127.0.0.1)
 *   port          - SIEM server port number (default: 8089)
 *   mode          - Reading mode: realtime|all|recent|range (default: realtime)
 *   hours_back    - Hours to look back for 'recent' mode (default: 24)
 *
 * Examples:
 *   log_forwarder.exe
 *   log_forwarder.exe 192.168.1.100
 *   log_forwarder.exe 192.168.1.100 8089
 *   log_forwarder.exe 192.168.1.100 8089 realtime
 *   log_forwarder.exe 192.168.1.100 8089 all
 *   log_forwarder.exe 192.168.1.100 8089 recent 12
 */

#include "forwarder_api.h"
#include "logger.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>

void printUsage() {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log Forwarder - Usage" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";
    std::cout << "Usage:" << std::endl;
    std::cout << "  log_forwarder.exe [server] [port] [mode] [hours]" << std::endl;
    std::cout << "\n";
    std::cout << "Arguments:" << std::endl;
    std::cout << "  server  - SIEM server address (default: 127.0.0.1)" << std::endl;
    std::cout << "  port    - SIEM server port (default: 8089)" << std::endl;
    std::cout << "  mode    - Reading mode (default: realtime)" << std::endl;
    std::cout << "            * realtime - Monitor future events only" << std::endl;
    std::cout << "            * all      - Read all historical events" << std::endl;
    std::cout << "            * recent   - Read recent events (last N hours)" << std::endl;
    std::cout << "  hours   - Hours to look back for 'recent' mode (default: 24)" << std::endl;
    std::cout << "\n";
    std::cout << "Examples:" << std::endl;
    std::cout << "  log_forwarder.exe" << std::endl;
    std::cout << "  log_forwarder.exe 192.168.1.100" << std::endl;
    std::cout << "  log_forwarder.exe 192.168.1.100 8089" << std::endl;
    std::cout << "  log_forwarder.exe 192.168.1.100 8089 realtime" << std::endl;
    std::cout << "  log_forwarder.exe 192.168.1.100 8089 all" << std::endl;
    std::cout << "  log_forwarder.exe 192.168.1.100 8089 recent 12" << std::endl;
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "\n";
}

/**
 * @brief Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, non-zero on error
 */
int main(int argc, char* argv[]) {
    // Show usage if --help or -h is provided
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h" || arg1 == "/?" || arg1 == "help") {
            printUsage();
            return 0;
        }
    }

    // Initialize logger first
    if (!initializeGlobalLogger("forwarder_logs.csv")) {
        std::cerr << "[Main] Failed to initialize logger" << std::endl;
        return 1;
    }r

    g_logger->info("Main", "Windows Event Log Forwarder starting", "");

    // Default configuration
    std::string serverAddress = DEFAULT_SIEM_SERVER;
    int serverPort = DEFAULT_SIEM_PORT;
    EventQueryConfig config;  // Defaults to REALTIME mode

    // Parse command-line arguments
    if (argc >= 2) {
        serverAddress = argv[1];
        g_logger->info("Main", "Using custom server address", serverAddress);
    }

    if (argc >= 3) {
        serverPort = std::atoi(argv[2]);
        g_logger->info("Main", "Using custom server port", std::to_string(serverPort));
    }

    if (argc >= 4) {
        std::string modeStr = argv[3];
        // Convert to lowercase for case-insensitive comparison
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

        if (modeStr == "realtime" || modeStr == "rt") {
            config.mode = EventReadMode::REALTIME;
            std::cout << "[Main] Mode: Real-time monitoring" << std::endl;
            g_logger->info("Main", "Mode set to REALTIME", "");
        } else if (modeStr == "all" || modeStr == "historical") {
            config.mode = EventReadMode::HISTORICAL_ALL;
            std::cout << "[Main] Mode: Historical (all events)" << std::endl;
            g_logger->info("Main", "Mode set to HISTORICAL_ALL", "");
        } else if (modeStr == "recent") {
            config.mode = EventReadMode::HISTORICAL_RECENT;
            config.hoursBack = 24;  // Default
            if (argc >= 5) {
                config.hoursBack = std::atoi(argv[4]);
            }
            std::cout << "[Main] Mode: Historical (last " << config.hoursBack << " hours)" << std::endl;
            g_logger->info("Main", "Mode set to HISTORICAL_RECENT",
                         "Hours back: " + std::to_string(config.hoursBack));
        } else {
            std::cerr << "[Main] Invalid mode: " << modeStr << std::endl;
            std::cerr << "[Main] Valid modes: realtime, all, recent" << std::endl;
            printUsage();
            g_logger->error("Main", "Invalid mode specified", modeStr);
            shutdownGlobalLogger();
            return 1;
        }
    }

    std::string target = serverAddress + ":" + std::to_string(serverPort);
    g_logger->info("Main", "Target SIEM server", target);

    // Run the forwarder (this call blocks for realtime, returns after completion for historical)
    int result = runForwarder(serverAddress, serverPort, config);

    // Shutdown logger on exit
    g_logger->info("Main", "Windows Event Log Forwarder shutting down",
                   "Exit code: " + std::to_string(result));
    shutdownGlobalLogger();

    return result;
}
