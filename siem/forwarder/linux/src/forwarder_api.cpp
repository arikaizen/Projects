/**
 * @file forwarder_api.cpp
 * @brief Implementation of main Linux System Log Forwarder API
 *
 * Provides high-level functions to initialize and run the log forwarding service.
 */

#include "forwarder_api.h"
#include "journal_reader.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <systemd/sd-journal.h>

void forwardSystemLogs(LogForwarder& forwarder) {
    sd_journal* journal = nullptr;
    int result;

    // Open the systemd journal
    result = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (result < 0) {
        std::cerr << "[JournalReader] Failed to open journal: " << strerror(-result) << std::endl;
        return;
    }

    // Seek to the end to get only new entries
    result = sd_journal_seek_tail(journal);
    if (result < 0) {
        std::cerr << "[JournalReader] Failed to seek to end of journal" << std::endl;
        sd_journal_close(journal);
        return;
    }

    std::cout << "[JournalReader] Successfully opened systemd journal" << std::endl;
    std::cout << "[JournalReader] Monitoring system logs..." << std::endl;

    // Main event processing loop
    while (true) {
        // Wait for new journal entries
        result = sd_journal_wait(journal, 1000000);  // 1 second timeout

        if (result < 0) {
            std::cerr << "[JournalReader] Error waiting for journal: " << strerror(-result) << std::endl;
            break;
        }

        // Process all available entries
        while (sd_journal_next(journal) > 0) {
            // Format entry as JSON
            std::string jsonLog = formatJournalEntryAsJson(journal);

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
        }

        // Small delay to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    sd_journal_close(journal);
}

int runForwarder(const std::string& serverAddress, int serverPort) {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Linux System Log Forwarder for SIEM" << std::endl;
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

    // Start monitoring system logs
    std::cout << "[ForwarderAPI] Starting system log monitoring..." << std::endl;
    forwardSystemLogs(forwarder);

    return 0;
}
