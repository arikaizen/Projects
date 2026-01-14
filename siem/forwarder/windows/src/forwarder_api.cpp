/**
 * @file forwarder_api.cpp
 * @brief Implementation of main Windows Event Log Forwarder API
 *
 * Provides high-level functions to initialize and run the log forwarding service
 * with support for both real-time and historical event reading.
 */

#include "forwarder_api.h"  // runForwarder, forwardWindowsLogs declarations
#include "event_log_reader.h" // formatEventAsJson, getEventProperty
#include "logger.h"         // g_logger usage
#include <windows.h>         // GetLastError, DWORD, ERROR_NO_MORE_ITEMS
#include <iostream>          // std::cout, std::cerr
#include <chrono>            // std::chrono::milliseconds
#include <thread>            // std::this_thread::sleep_for

void forwardWindowsLogs(LogForwarder& forwarder, const std::wstring& channelPath,
                        const EventQueryConfig& config) {
    EVT_HANDLE hSubscription = NULL;
    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;
    int eventCount = 0;

    // Build query based on configuration
    std::wstring query = buildHistoricalQuery(config);

    // Determine subscription mode based on configuration
    if (config.mode == EventReadMode::REALTIME) {
        // Subscribe to future events only (real-time monitoring)
        std::cout << "[EventLogReader] Mode: REAL-TIME monitoring" << std::endl;
        if (g_logger) {
            g_logger->info("EventLogReader", "Mode: Real-time monitoring", "");
        }

        // For real-time monitoring with pull model, use EvtQuery instead of EvtSubscribe
        // to avoid ERROR_INVALID_PARAMETER (error 87)
        hSubscription = EvtQuery(
            NULL,                           // Session (NULL = localhost)
            channelPath.c_str(),            // Channel path
            L"*",                           // Wildcard to match all events
            EvtQueryChannelPath | EvtQueryReverseDirection
        );

        // Seek to the end to only capture new events (simulating EvtSubscribeToFutureEvents)
        if (hSubscription != NULL) {
            EvtSeek(hSubscription, 0, NULL, 0, EvtSeekRelativeToLast);
        }
    } else {
        // Query historical events
        const wchar_t* modeStr = L"UNKNOWN";
        std::string modeDetail = "";

        switch (config.mode) {
            case EventReadMode::HISTORICAL_ALL:
                modeStr = L"HISTORICAL (All Events)";
                modeDetail = "Reading all historical events";
                break;
            case EventReadMode::HISTORICAL_RECENT:
                modeStr = L"HISTORICAL (Recent)";
                modeDetail = "Reading events from last " + std::to_string(config.hoursBack) + " hours";
                break;
            case EventReadMode::HISTORICAL_RANGE:
                modeStr = L"HISTORICAL (Time Range)";
                modeDetail = "Reading events within specified time range";
                break;
            default:
                break;
        }

        std::wcout << L"[EventLogReader] Mode: " << modeStr << std::endl;
        if (g_logger) {
            g_logger->info("EventLogReader", "Mode: Historical query", modeDetail);
        }

        hSubscription = EvtQuery(
            NULL,                           // Session (NULL = localhost)
            channelPath.c_str(),            // Channel path
            query.c_str(),                  // XPath query with time filtering
            EvtQueryChannelPath | EvtQueryForwardDirection
        );
    }

    if (nullptr == hSubscription) {
        // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
        DWORD error = GetLastError();
        std::cerr << "[EventLogReader] Failed to subscribe/query event log channel" << std::endl;
        std::cerr << "[EventLogReader] Error code: " << error << std::endl;
        std::cerr << "[EventLogReader] Tip: Run as Administrator to access Security logs" << std::endl;
        if (g_logger) {
            g_logger->error("EventLogReader", "Failed to subscribe/query event log channel",
                          "Error code: " + std::to_string(error) + " - Run as Administrator");
        }
        return;
    }

    std::cout << "[EventLogReader] Successfully subscribed/queried event log channel" << std::endl;
    if (config.mode == EventReadMode::REALTIME) {
        std::cout << "[EventLogReader] Monitoring Windows Event Logs (real-time)..." << std::endl;
    } else {
        std::cout << "[EventLogReader] Reading Windows Event Logs (historical)..." << std::endl;
    }

    if (g_logger) {
        g_logger->info("EventLogReader", "Successfully subscribed/queried event log",
                      config.mode == EventReadMode::REALTIME ? "Real-time mode" : "Historical mode");
    }

    // Main event processing loop
    bool continueProcessing = true;
    while (continueProcessing) {
        // For real-time mode, wait indefinitely. For historical mode, use short timeout
        DWORD timeout = (config.mode == EventReadMode::REALTIME) ? INFINITE : 5000;

        // Get next batch of events (up to 10)
        if (EvtNext(hSubscription, 10, hEvents, timeout, 0, &dwReturned)) {
            // Process each event in the batch
            if (dwReturned > 0) {
            for (DWORD i = 0; i < dwReturned; i++) {
                // Format event as JSON
                std::string jsonLog = formatEventAsJson(hEvents[i]);

                // Check connection status and reconnect if needed
                if (!forwarder.isConnected()) {
                    std::cout << "[ForwarderAPI] Connection lost, attempting to reconnect..." << std::endl;
                    if (g_logger) {
                        g_logger->warning("ForwarderAPI", "Connection lost, attempting reconnection", "");
                    }
                    if (!forwarder.connect()) {
                        std::cerr << "[ForwarderAPI] Reconnection failed, waiting "
                                  << RECONNECT_DELAY_MS << "ms before retry..." << std::endl;
                        if (g_logger) {
                            g_logger->warning("ForwarderAPI", "Reconnection failed",
                                            "Waiting " + std::to_string(RECONNECT_DELAY_MS) + "ms");
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                        continue;
                    }
                }

                // Forward log to SIEM server
                if (forwarder.sendLog(jsonLog)) {
                    eventCount++;
                    std::cout << "[ForwarderAPI] Forwarded (" << eventCount << "): " << jsonLog << std::endl;
                    if (g_logger) {
                        g_logger->info("ForwarderAPI", "Event forwarded successfully",
                                     "Total: " + std::to_string(eventCount));
                    }
                } else {
                    std::cerr << "[ForwarderAPI] Failed to forward log" << std::endl;
                    if (g_logger) {
                        g_logger->error("ForwarderAPI", "Failed to forward event", "");
                    }
                }

                // Close event handle
                // EvtClose: Close event log handle and release resources. Returns TRUE on success, FALSE on failure.
                EvtClose(hEvents[i]);
            }
            }
        } else {
            // Handle EvtNext failure
            // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
            DWORD status = GetLastError();
            if (status == ERROR_NO_MORE_ITEMS) {
                // For historical queries, this means we've read all matching events
                if (config.mode != EventReadMode::REALTIME) {
                    std::cout << "[EventLogReader] Finished reading historical events" << std::endl;
                    std::cout << "[EventLogReader] Total events forwarded: " << eventCount << std::endl;
                    if (g_logger) {
                        g_logger->info("EventLogReader", "Finished reading historical events",
                                     "Total forwarded: " + std::to_string(eventCount));
                    }
                    continueProcessing = false;
                }
            } else if (status == ERROR_TIMEOUT) {
                // Timeout in historical mode - check if there are more events
                if (config.mode != EventReadMode::REALTIME) {
                    std::cout << "[EventLogReader] Query timeout - assuming no more events" << std::endl;
                    std::cout << "[EventLogReader] Total events forwarded: " << eventCount << std::endl;
                    if (g_logger) {
                        g_logger->info("EventLogReader", "Query timeout - finished",
                                     "Total forwarded: " + std::to_string(eventCount));
                    }
                    continueProcessing = false;
                }
            } else {
                std::cerr << "[EventLogReader] EvtNext failed with error: " << status << std::endl;
                if (g_logger) {
                    g_logger->error("EventLogReader", "EvtNext failed",
                                  "Error code: " + std::to_string(status));
                }
            }
        }

        // Small delay to prevent CPU spinning (only in real-time mode)
        if (config.mode == EventReadMode::REALTIME) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Cleanup subscription handle
    if (hSubscription) {
        // EvtClose: Close event log handle and release resources. Returns TRUE on success, FALSE on failure.
        EvtClose(hSubscription);
    }
}

int runForwarder(const std::string& serverAddress, int serverPort,
                 const EventQueryConfig& config) {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log Forwarder for SIEM" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server: " << serverAddress << ":" << serverPort << std::endl;

    // Display mode information
    std::cout << "Mode: ";
    switch (config.mode) {
        case EventReadMode::REALTIME:
            std::cout << "Real-Time Monitoring";
            break;
        case EventReadMode::HISTORICAL_ALL:
            std::cout << "Historical (All Events)";
            break;
        case EventReadMode::HISTORICAL_RECENT:
            std::cout << "Historical (Last " << config.hoursBack << " hours)";
            break;
        case EventReadMode::HISTORICAL_RANGE:
            std::cout << "Historical (Time Range)";
            break;
    }
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";

    // Create and initialize forwarder
    LogForwarder forwarder(serverAddress, serverPort);

    if (!forwarder.initialize()) {
        std::cerr << "[ForwarderAPI] Failed to initialize forwarder" << std::endl;
        if (g_logger) {
            g_logger->error("ForwarderAPI", "Failed to initialize forwarder", "");
        }
        return 1;
    }

    // Attempt initial connection with retry logic
    std::cout << "[ForwarderAPI] Attempting to connect to SIEM server..." << std::endl;
    if (g_logger) {
        g_logger->info("ForwarderAPI", "Attempting initial connection to SIEM server",
                      serverAddress + ":" + std::to_string(serverPort));
    }

    while (!forwarder.connect()) {
        std::cout << "[ForwarderAPI] Connection failed, retrying in "
                  << RECONNECT_DELAY_MS << "ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
    }

    std::cout << "[ForwarderAPI] Connection established successfully!" << std::endl;
    std::cout << "\n";
    if (g_logger) {
        g_logger->info("ForwarderAPI", "Initial connection established successfully", "");
    }

    // Start monitoring or querying Windows Event Logs
    std::cout << "[ForwarderAPI] Starting event log processing..." << std::endl;
    if (g_logger) {
        std::string modeStr = (config.mode == EventReadMode::REALTIME) ? "Real-time" : "Historical";
        g_logger->info("ForwarderAPI", "Starting event log processing",
                      "Mode: " + modeStr + " | Channel: System");
    }
    forwardWindowsLogs(forwarder, L"System", config);

    std::cout << "[ForwarderAPI] Event log processing completed" << std::endl;
    if (g_logger) {
        g_logger->info("ForwarderAPI", "Event log processing completed", "");
    }

    return 0;
}
