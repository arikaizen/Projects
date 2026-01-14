/**
 * @file test_realtime_console.cpp
 * @brief Test program to read Windows Event Logs in real-time and print to console
 *
 * This test demonstrates real-time event monitoring without requiring a SIEM server.
 * Events are formatted as JSON and printed to the console as they occur.
 *
 * Usage:
 *   test_realtime_console.exe [channel] [mode]
 *
 * Arguments:
 *   channel - Event Log channel (default: System)
 *             Options: System, Application, Security
 *   mode    - Reading mode (default: realtime)
 *             Options: realtime, all, recent
 *
 * Examples:
 *   test_realtime_console.exe
 *   test_realtime_console.exe System realtime
 *   test_realtime_console.exe Application recent
 */

#include "event_log_reader.h"
#include "logger.h"
#include <windows.h>
#include <winevt.h>
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

// ANSI color codes for console output
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

void printBanner() {
    std::cout << "\n";
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "  Windows Event Log Real-Time Monitor  " << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "\n";
}

void printUsage() {
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  test_realtime_console.exe [channel] [mode]" << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  channel - Event Log channel (default: System)" << std::endl;
    std::cout << "            Options: System, Application, Security" << std::endl;
    std::cout << "  mode    - Reading mode (default: realtime)" << std::endl;
    std::cout << "            Options: realtime, all, recent" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  test_realtime_console.exe" << std::endl;
    std::cout << "  test_realtime_console.exe System realtime" << std::endl;
    std::cout << "  test_realtime_console.exe Application all" << std::endl;
    std::cout << "  test_realtime_console.exe Security recent" << std::endl;
    std::cout << "\n";
}

/**
 * @brief Monitor Windows Event Logs and print to console
 *
 * @param channelPath Event Log channel to monitor
 * @param config Query configuration (real-time or historical)
 */
void monitorEventsToConsole(const std::wstring& channelPath, const EventQueryConfig& config) {
    EVT_HANDLE hQuery = NULL;
    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;
    int eventCount = 0;

    std::wcout << COLOR_GREEN << L"[Monitor] Channel: " << channelPath << COLOR_RESET << std::endl;

    // For real-time monitoring, use polling with EvtQuery
    if (config.mode == EventReadMode::REALTIME) {
        std::cout << COLOR_GREEN << "[Monitor] Mode: REAL-TIME (Future events only)" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Waiting for new events... (Press Ctrl+C to stop)" << COLOR_RESET << std::endl;
        std::cout << "\n";
        std::cout << COLOR_GREEN << "[Monitor] Successfully started monitoring" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        auto startTime = std::chrono::steady_clock::now();
        // Start from 2 seconds ago to catch any events happening right as we start
        std::wstring lastTimestamp = getTimeString(-2);

        // Real-time polling loop
        while (true) {
            // Build query for events after last timestamp
            std::wostringstream queryStream;
            queryStream << L"*[System[TimeCreated[@SystemTime>'" << lastTimestamp << L"']]]";
            std::wstring query = queryStream.str();

            // Query for new events
            hQuery = EvtQuery(
                NULL,
                channelPath.c_str(),
                query.c_str(),
                EvtQueryChannelPath | EvtQueryForwardDirection
            );

            if (hQuery != NULL) {
                // Read events from this query
                while (EvtNext(hQuery, 10, hEvents, 1000, 0, &dwReturned)) {
                    for (DWORD i = 0; i < dwReturned; i++) {
                        eventCount++;

                        // Format event as JSON
                        std::string jsonLog = formatEventAsJson(hEvents[i]);

                        // Get current timestamp
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                        // Print event with color
                        std::cout << COLOR_MAGENTA << "[Event #" << eventCount
                                 << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
                        std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
                        std::cout << "\n";

                        // Close event handle
                        EvtClose(hEvents[i]);
                    }
                }

                EvtClose(hQuery);
                hQuery = NULL;
            }

            // Update timestamp for next query
            lastTimestamp = getTimeString(0);

            // Sleep briefly before next poll (500ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    } else {
        // Historical mode
        std::cout << COLOR_GREEN << "[Monitor] Mode: HISTORICAL" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Reading historical events..." << COLOR_RESET << std::endl;
        std::cout << "\n";

        std::wstring query = buildHistoricalQuery(config);
        hQuery = EvtQuery(
            NULL,
            channelPath.c_str(),
            query.c_str(),
            EvtQueryChannelPath | EvtQueryForwardDirection
        );

        if (hQuery == NULL) {
            DWORD error = GetLastError();
            std::cerr << COLOR_RED << "[ERROR] Failed to query event log" << COLOR_RESET << std::endl;
            std::cerr << COLOR_RED << "[ERROR] Error code: " << error << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "[Monitor] Successfully started query" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        auto startTime = std::chrono::steady_clock::now();
        bool continueProcessing = true;

        while (continueProcessing) {
            if (EvtNext(hQuery, 10, hEvents, 5000, 0, &dwReturned)) {
                for (DWORD i = 0; i < dwReturned; i++) {
                    eventCount++;

                    // Format event as JSON
                    std::string jsonLog = formatEventAsJson(hEvents[i]);

                    // Get current timestamp
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                    // Print event with color
                    std::cout << COLOR_MAGENTA << "[Event #" << eventCount
                             << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
                    std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
                    std::cout << "\n";

                    // Close event handle
                    EvtClose(hEvents[i]);
                }
            } else {
                DWORD status = GetLastError();

                if (status == ERROR_NO_MORE_ITEMS) {
                    std::cout << COLOR_GREEN << "[Monitor] Finished reading historical events" << COLOR_RESET << std::endl;
                    std::cout << COLOR_GREEN << "[Monitor] Total events read: " << eventCount << COLOR_RESET << std::endl;
                    continueProcessing = false;
                } else if (status == ERROR_TIMEOUT) {
                    std::cout << COLOR_GREEN << "[Monitor] Query timeout - finished" << COLOR_RESET << std::endl;
                    std::cout << COLOR_GREEN << "[Monitor] Total events read: " << eventCount << COLOR_RESET << std::endl;
                    continueProcessing = false;
                } else {
                    std::cerr << COLOR_RED << "[ERROR] EvtNext failed with error: " << status << COLOR_RESET << std::endl;
                    continueProcessing = false;
                }
            }
        }

        // Cleanup
        if (hQuery) {
            EvtClose(hQuery);
        }

        std::cout << "\n";
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "[Monitor] Monitoring session complete" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Enable ANSI color codes on Windows 10+
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printBanner();

    // Check for help flag
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h" || arg1 == "/?" || arg1 == "help") {
            printUsage();
            return 0;
        }
    }

    // Initialize logger (to suppress logger warnings in event_log_reader)
    initializeGlobalLogger("test_realtime_console.csv");

    // Default configuration
    std::wstring channelPath = L"System";
    EventQueryConfig config;  // Defaults to REALTIME mode

    // Parse channel argument
    if (argc >= 2) {
        std::string channelStr = argv[1];

        if (channelStr == "System" || channelStr == "system") {
            channelPath = L"System";
        } else if (channelStr == "Application" || channelStr == "application") {
            channelPath = L"Application";
        } else if (channelStr == "Security" || channelStr == "security") {
            channelPath = L"Security";
        } else {
            // Try to use as-is (for custom channels)
            channelPath = std::wstring(channelStr.begin(), channelStr.end());
        }
    }

    // Parse mode argument
    if (argc >= 3) {
        std::string modeStr = argv[2];
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

        if (modeStr == "realtime" || modeStr == "rt") {
            config.mode = EventReadMode::REALTIME;
        } else if (modeStr == "all" || modeStr == "historical") {
            config.mode = EventReadMode::HISTORICAL_ALL;
        } else if (modeStr == "recent") {
            config.mode = EventReadMode::HISTORICAL_RECENT;
            config.hoursBack = 24;  // Default
            if (argc >= 4) {
                config.hoursBack = std::atoi(argv[3]);
            }
        } else {
            std::cerr << COLOR_RED << "[ERROR] Invalid mode: " << modeStr << COLOR_RESET << std::endl;
            printUsage();
            shutdownGlobalLogger();
            return 1;
        }
    }

    // Start monitoring
    monitorEventsToConsole(channelPath, config);

    // Cleanup
    shutdownGlobalLogger();

    return 0;
}
