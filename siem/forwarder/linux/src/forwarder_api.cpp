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

void forwardSystemLogs(LogForwarder& forwarder, const LogQueryConfig& config) {
    sd_journal* journal = nullptr;
    int result;
    int eventCount = 0;

    std::cout << "[JournalReader] Source: systemd-journal" << std::endl;

    // Open the systemd journal
    result = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (result < 0) {
        std::cerr << "[JournalReader] Failed to open journal: " << strerror(-result) << std::endl;
        return;
    }

    // Apply filters if specified
    for (const auto& unit : config.units) {
        sd_journal_add_match(journal, ("_SYSTEMD_UNIT=" + unit).c_str(), 0);
        std::cout << "[JournalReader] Filtering by unit: " << unit << std::endl;
    }

    // Apply priority filter if specified
    if (config.minPriority >= 0 && config.minPriority <= 7) {
        std::string match = "PRIORITY=" + std::to_string(config.minPriority);
        sd_journal_add_match(journal, match.c_str(), 0);
        std::cout << "[JournalReader] Filtering by min priority: " << config.minPriority << std::endl;
    }

    if (config.mode == LogReadMode::REALTIME) {
        std::cout << "[JournalReader] Mode: REAL-TIME monitoring" << std::endl;

        // Seek to the end for real-time monitoring
        result = sd_journal_seek_tail(journal);
        if (result < 0) {
            std::cerr << "[JournalReader] Failed to seek to end of journal" << std::endl;
            sd_journal_close(journal);
            return;
        }
        sd_journal_previous(journal);  // Move to last entry

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
                eventCount++;

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
                    std::cout << "[ForwarderAPI] Forwarded (" << eventCount << "): " << jsonLog << std::endl;
                } else {
                    std::cerr << "[ForwarderAPI] Failed to forward log" << std::endl;
                }
            }

            // Small delay to prevent CPU spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        // Historical mode
        std::cout << "[JournalReader] Mode: HISTORICAL" << std::endl;

        // Seek to beginning or specific time
        if (config.mode == LogReadMode::HISTORICAL_ALL) {
            sd_journal_seek_head(journal);
            std::cout << "[JournalReader] Reading all historical entries" << std::endl;
        } else if (config.mode == LogReadMode::HISTORICAL_RECENT) {
            uint64_t cutoff = getTimestamp(-config.hoursBack) * 1000000;
            sd_journal_seek_realtime_usec(journal, cutoff);
            std::cout << "[JournalReader] Reading entries from last " << config.hoursBack << " hours" << std::endl;
        }

        std::cout << "[JournalReader] Processing historical logs..." << std::endl;

        // Read all matching entries
        while (sd_journal_next(journal) > 0) {
            eventCount++;

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
                std::cout << "[ForwarderAPI] Forwarded (" << eventCount << "): " << jsonLog << std::endl;
            } else {
                std::cerr << "[ForwarderAPI] Failed to forward log" << std::endl;
            }
        }

        std::cout << "[JournalReader] Finished reading historical entries" << std::endl;
        std::cout << "[JournalReader] Total entries forwarded: " << eventCount << std::endl;
    }

    // Cleanup
    sd_journal_close(journal);
}

int runForwarder(const std::string& serverAddress, int serverPort, const LogQueryConfig& config) {
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
    forwardSystemLogs(forwarder, config);

    return 0;
}
