/**
 * @file forwarder_api.cpp
 * @brief Implementation of main Windows Event Log Forwarder API
 *
 * Provides high-level functions to initialize and run the log forwarding service.
 */

#include "../inc/forwarder_api.h"
#include "../inc/event_log_reader.h"
#include <iostream>
#include <chrono>
#include <thread>

void forwardWindowsLogs(LogForwarder& forwarder, const std::wstring& channelPath) {
    EVT_HANDLE hSubscription = NULL;
    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;

    // Subscribe to the Windows Event Log channel
    hSubscription = EvtSubscribe(
        NULL,                           // Session (NULL = localhost)
        NULL,                           // Signal event (NULL = use EvtNext)
        channelPath.c_str(),            // Channel path
        L"*",                           // Query (all events)
        NULL,                           // Bookmark
        NULL,                           // Context
        NULL,                           // Callback
        EvtSubscribeToFutureEvents      // Subscribe to future events only
    );

    if (hSubscription == NULL) {
        std::cerr << "[EventLogReader] Failed to subscribe to event log channel" << std::endl;
        std::cerr << "[EventLogReader] Error code: " << GetLastError() << std::endl;
        std::cerr << "[EventLogReader] Tip: Run as Administrator to access Security logs" << std::endl;
        return;
    }

    std::cout << "[EventLogReader] Successfully subscribed to event log channel" << std::endl;
    std::cout << "[EventLogReader] Monitoring Windows Event Logs..." << std::endl;

    // Main event processing loop
    while (true) {
        // Get next batch of events (up to 10)
        if (EvtNext(hSubscription, 10, hEvents, INFINITE, 0, &dwReturned)) {
            // Process each event in the batch
            for (DWORD i = 0; i < dwReturned; i++) {
                // Format event as JSON
                std::string jsonLog = formatEventAsJson(hEvents[i]);

                // Check connection status and reconnect if needed
                if (!forwarder.isConnected()) {
                    std::cout << "[ForwarderAPI] Connection lost, attempting to reconnect..." << std::endl;
                    if (!forwarder.connect()) {
                        std::cerr << "[ForwarderAPI] Reconnection failed, waiting "
                                  << RECONNECT_DELAY_MS << "ms before retry..." << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                        continue;
                    }
                }

                // Forward log to SIEM server
                if (forwarder.sendLog(jsonLog)) {
                    std::cout << "[ForwarderAPI] Forwarded: " << jsonLog << std::endl;
                } else {
                    std::cerr << "[ForwarderAPI] Failed to forward log" << std::endl;
                }

                // Close event handle
                EvtClose(hEvents[i]);
            }
        } else {
            // Handle EvtNext failure
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
        return 1;
    }

    // Attempt initial connection with retry logic
    std::cout << "[ForwarderAPI] Attempting to connect to SIEM server..." << std::endl;
    while (!forwarder.connect()) {
        std::cout << "[ForwarderAPI] Connection failed, retrying in "
                  << RECONNECT_DELAY_MS << "ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
    }

    std::cout << "[ForwarderAPI] Connection established successfully!" << std::endl;
    std::cout << "\n";

    // Start monitoring Windows Event Logs
    // Currently monitoring System channel only
    // Future enhancement: spawn threads for multiple channels
    std::cout << "[ForwarderAPI] Starting event log monitoring..." << std::endl;
    forwardWindowsLogs(forwarder, L"System");

    return 0;
}
