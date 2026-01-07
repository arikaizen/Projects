/**
 * @file forwarder_api.cpp
 * @brief Implementation of main Windows Event Log Forwarder API
 *
 * Provides high-level functions to initialize and run the log forwarding service.
 */

#include "forwarder_api.h"  // runForwarder, forwardWindowsLogs declarations
#include "event_log_reader.h" // formatEventAsJson, getEventProperty
#include "logger.h"         // g_logger usage
#include <windows.h>         // GetLastError, DWORD, ERROR_NO_MORE_ITEMS
#include <iostream>          // std::cout, std::cerr
#include <chrono>            // std::chrono::milliseconds
#include <thread>            // std::this_thread::sleep_for

void forwardWindowsLogs(LogForwarder& forwarder, const std::wstring& channelPath) {
    EVT_HANDLE hSubscription = nullptr;
    EVT_HANDLE hEvents[10] = {nullptr};
    DWORD dwReturned = 0;

    // Subscribe to the Windows Event Log channel
    // EvtSubscribe: Create a subscription to Windows Event Log channel. Returns EVT_HANDLE on success, NULL on failure.
    hSubscription = EvtSubscribe(
        nullptr,                        // Session (NULL = localhost)
        nullptr,                        // Signal event (NULL = use EvtNext)
        channelPath.c_str(),            // Channel path
        L"*",                           // Query (all events)
        nullptr,                        // Bookmark
        nullptr,                        // Context
        nullptr,                        // Callback
        EvtSubscribeToFutureEvents      // Subscribe to future events only
    );

    if (nullptr == hSubscription) {
        // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
        DWORD error = GetLastError();
        std::cerr << "[EventLogReader] Failed to subscribe to event log channel" << std::endl;
        std::cerr << "[EventLogReader] Error code: " << error << std::endl;
        std::cerr << "[EventLogReader] Tip: Run as Administrator to access Security logs" << std::endl;
        if (g_logger) {
            g_logger->error("EventLogReader", "Failed to subscribe to event log channel",
                          "Error code: " + std::to_string(error) + " - Run as Administrator");
        }
        return;
    }

    std::cout << "[EventLogReader] Successfully subscribed to event log channel" << std::endl;
    std::cout << "[EventLogReader] Monitoring Windows Event Logs..." << std::endl;
    if (g_logger) {
        g_logger->info("EventLogReader", "Successfully subscribed to event log",
                      "Monitoring for future events");
    }

    // Main event processing loop
    while (true) {
        // Get next batch of events (up to 10)
        // EvtNext: Retrieve next batch of events from subscription. Returns TRUE if events found, FALSE otherwise.
        if (EvtNext(hSubscription, 10, hEvents, INFINITE, 0, &dwReturned)) {
            // Process each event in the batch
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
                    std::cout << "[ForwarderAPI] Forwarded: " << jsonLog << std::endl;
                    if (g_logger) {
                        g_logger->info("ForwarderAPI", "Event forwarded successfully", "");
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
        } else {
            // Handle EvtNext failure
            // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
            DWORD status = GetLastError();
            if (status != ERROR_NO_MORE_ITEMS) {
                std::cerr << "[EventLogReader] EvtNext failed with error: " << status << std::endl;
            }
        }

        // Small delay to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup subscription handle
    if (hSubscription) {
        // EvtClose: Close event log handle and release resources. Returns TRUE on success, FALSE on failure.
        EvtClose(hSubscription);
    }
}

int runForwarder(const std::string& serverAddress, int serverPort) {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log Forwarder for SIEM" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Server: " << serverAddress << ":" << serverPort << std::endl;
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

    // Start monitoring Windows Event Logs
    // Currently monitoring System channel only
    // Future enhancement: spawn threads for multiple channels
    std::cout << "[ForwarderAPI] Starting event log monitoring..." << std::endl;
    if (g_logger) {
        g_logger->info("ForwarderAPI", "Starting event log monitoring", "Channel: System");
    }
    forwardWindowsLogs(forwarder, L"System");

    return 0;
}
