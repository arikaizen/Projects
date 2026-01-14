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
#include <sstream>           // std::wostringstream
#include <chrono>            // std::chrono::milliseconds
#include <thread>            // std::this_thread::sleep_for

void forwardWindowsLogs(LogForwarder& forwarder, const std::wstring& channelPath,
                        const EventQueryConfig& config) {
    EVT_HANDLE hQuery = NULL;
    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;
    int eventCount = 0;

    // For real-time monitoring, use polling with EvtQuery
    if (config.mode == EventReadMode::REALTIME) {
        std::cout << "[EventLogReader] Mode: REAL-TIME monitoring" << std::endl;
        if (g_logger) {
            g_logger->info("EventLogReader", "Mode: Real-time monitoring", "");
        }

        std::cout << "[EventLogReader] Successfully started real-time monitoring" << std::endl;
        std::cout << "[EventLogReader] Monitoring Windows Event Logs (real-time)..." << std::endl;
        if (g_logger) {
            g_logger->info("EventLogReader", "Real-time monitoring started", "");
        }

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
                                EvtClose(hEvents[i]);
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
        // Historical mode - query once
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

        std::wstring query = buildHistoricalQuery(config);
        hQuery = EvtQuery(
            NULL,
            channelPath.c_str(),
            query.c_str(),
            EvtQueryChannelPath | EvtQueryForwardDirection
        );

        if (nullptr == hQuery) {
            DWORD error = GetLastError();
            std::cerr << "[EventLogReader] Failed to query event log channel" << std::endl;
            std::cerr << "[EventLogReader] Error code: " << error << std::endl;
            if (g_logger) {
                g_logger->error("EventLogReader", "Failed to query event log channel",
                              "Error code: " + std::to_string(error));
            }
            return;
        }

        std::cout << "[EventLogReader] Successfully queried event log channel" << std::endl;
        std::cout << "[EventLogReader] Reading Windows Event Logs (historical)..." << std::endl;
        if (g_logger) {
            g_logger->info("EventLogReader", "Successfully queried event log", "Historical mode");
        }

        // Process historical events
        bool continueProcessing = true;
        while (continueProcessing) {
            if (EvtNext(hQuery, 10, hEvents, 5000, 0, &dwReturned)) {
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
                                EvtClose(hEvents[i]);
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
                        EvtClose(hEvents[i]);
                    }
                }
            } else {
                // Handle EvtNext failure
                DWORD status = GetLastError();
                if (status == ERROR_NO_MORE_ITEMS) {
                    std::cout << "[EventLogReader] Finished reading historical events" << std::endl;
                    std::cout << "[EventLogReader] Total events forwarded: " << eventCount << std::endl;
                    if (g_logger) {
                        g_logger->info("EventLogReader", "Finished reading historical events",
                                     "Total forwarded: " + std::to_string(eventCount));
                    }
                    continueProcessing = false;
                } else if (status == ERROR_TIMEOUT) {
                    std::cout << "[EventLogReader] Query timeout - assuming no more events" << std::endl;
                    std::cout << "[EventLogReader] Total events forwarded: " << eventCount << std::endl;
                    if (g_logger) {
                        g_logger->info("EventLogReader", "Query timeout - finished",
                                     "Total forwarded: " + std::to_string(eventCount));
                    }
                    continueProcessing = false;
                } else {
                    std::cerr << "[EventLogReader] EvtNext failed with error: " << status << std::endl;
                    if (g_logger) {
                        g_logger->error("EventLogReader", "EvtNext failed",
                                      "Error code: " + std::to_string(status));
                    }
                    continueProcessing = false;
                }
            }
        }

        // Cleanup query handle
        if (hQuery) {
            EvtClose(hQuery);
        }
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
