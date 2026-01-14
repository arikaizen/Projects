/**
 * @file test_realtime_console.cpp
 * @brief Test program to read Linux system logs in real-time and print to console
 *
 * This test demonstrates real-time log monitoring without requiring a SIEM server.
 * Logs are formatted as JSON and printed to the console as they occur.
 *
 * Usage:
 *   test_realtime_console [source] [mode]
 *
 * Arguments:
 *   source - Log source (default: journal)
 *            Options: journal, syslog, auth, kern
 *   mode   - Reading mode (default: realtime)
 *            Options: realtime, all, recent
 *
 * Examples:
 *   test_realtime_console
 *   test_realtime_console journal realtime
 *   test_realtime_console auth recent
 *   test_realtime_console syslog all
 */

#include "journal_reader.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <fstream>
#include <systemd/sd-journal.h>
#include <sys/inotify.h>
#include <unistd.h>

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
    std::cout << COLOR_BLUE << "  Linux System Log Real-Time Monitor   " << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "\n";
}

void printUsage() {
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  test_realtime_console [source] [mode]" << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  source - Log source (default: journal)" << std::endl;
    std::cout << "           Options: journal, syslog, auth, kern" << std::endl;
    std::cout << "  mode   - Reading mode (default: realtime)" << std::endl;
    std::cout << "           Options: realtime, all, recent" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  test_realtime_console" << std::endl;
    std::cout << "  test_realtime_console journal realtime" << std::endl;
    std::cout << "  test_realtime_console auth all" << std::endl;
    std::cout << "  test_realtime_console syslog recent" << std::endl;
    std::cout << "\n";
}

/**
 * @brief Monitor systemd journal and print to console
 */
void monitorJournalToConsole(const LogQueryConfig& config) {
    sd_journal* journal = nullptr;
    int result;
    int eventCount = 0;

    std::cout << COLOR_GREEN << "[Monitor] Source: systemd-journal" << COLOR_RESET << std::endl;

    // Open the systemd journal
    result = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (result < 0) {
        std::cerr << COLOR_RED << "[ERROR] Failed to open journal: " << strerror(-result) << COLOR_RESET << std::endl;
        return;
    }

    // Apply filters if specified
    for (const auto& unit : config.units) {
        sd_journal_add_match(journal, ("_SYSTEMD_UNIT=" + unit).c_str(), 0);
    }

    auto startTime = std::chrono::steady_clock::now();

    if (config.mode == LogReadMode::REALTIME) {
        std::cout << COLOR_GREEN << "[Monitor] Mode: REAL-TIME (Future entries only)" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Waiting for new log entries... (Press Ctrl+C to stop)" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Seek to tail for real-time monitoring
        sd_journal_seek_tail(journal);
        sd_journal_previous(journal);  // Move to last entry

        std::cout << COLOR_GREEN << "[Monitor] Successfully started monitoring" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Real-time monitoring loop
        while (true) {
            result = sd_journal_wait(journal, 1000000);  // 1 second timeout

            if (result < 0) {
                std::cerr << COLOR_RED << "[ERROR] Error waiting for journal: " << strerror(-result) << COLOR_RESET << std::endl;
                break;
            }

            // Process all available entries
            while (sd_journal_next(journal) > 0) {
                eventCount++;

                // Format entry as JSON
                std::string jsonLog = formatJournalEntryAsJson(journal);

                // Get current timestamp
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                // Print entry with color
                std::cout << COLOR_MAGENTA << "[Entry #" << eventCount
                         << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
                std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
                std::cout << "\n";
            }
        }
    } else {
        std::cout << COLOR_GREEN << "[Monitor] Mode: HISTORICAL" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Reading historical entries..." << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Seek to beginning for historical reading
        if (config.mode == LogReadMode::HISTORICAL_ALL) {
            sd_journal_seek_head(journal);
        } else if (config.mode == LogReadMode::HISTORICAL_RECENT) {
            // Seek to recent entries (last N hours)
            uint64_t cutoff = getTimestamp(-config.hoursBack) * 1000000;  // Convert to microseconds
            sd_journal_seek_realtime_usec(journal, cutoff);
        }

        std::cout << COLOR_GREEN << "[Monitor] Successfully started reading" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Read all entries
        while (sd_journal_next(journal) > 0) {
            eventCount++;

            // Format entry as JSON
            std::string jsonLog = formatJournalEntryAsJson(journal);

            // Get current timestamp
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

            // Print entry with color
            std::cout << COLOR_MAGENTA << "[Entry #" << eventCount
                     << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
            std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
            std::cout << "\n";
        }

        std::cout << COLOR_GREEN << "[Monitor] Finished reading historical entries" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "[Monitor] Total entries read: " << eventCount << COLOR_RESET << std::endl;
    }

    // Cleanup
    sd_journal_close(journal);

    std::cout << "\n";
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "[Monitor] Monitoring session complete" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "\n";
}

/**
 * @brief Monitor log file and print to console (tail -f style)
 */
void monitorLogFileToConsole(const LogQueryConfig& config) {
    std::string logPath = getLogFilePath(config.source);
    if (config.source == LogSource::CUSTOM_FILE) {
        logPath = config.customPath;
    }

    std::cout << COLOR_GREEN << "[Monitor] Source: " << logPath << COLOR_RESET << std::endl;

    std::ifstream logFile(logPath);
    if (!logFile.is_open()) {
        std::cerr << COLOR_RED << "[ERROR] Failed to open log file: " << logPath << COLOR_RESET << std::endl;
        std::cerr << COLOR_YELLOW << "[TIP] Run as root/sudo to access system log files" << COLOR_RESET << std::endl;
        return;
    }

    int eventCount = 0;
    auto startTime = std::chrono::steady_clock::now();
    std::string sourceName = (config.source == LogSource::SYSLOG_FILE) ? "syslog" :
                             (config.source == LogSource::AUTH_LOG_FILE) ? "auth.log" :
                             (config.source == LogSource::KERN_LOG_FILE) ? "kern.log" : "custom";

    if (config.mode == LogReadMode::REALTIME) {
        std::cout << COLOR_GREEN << "[Monitor] Mode: REAL-TIME (New entries only)" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Waiting for new log entries... (Press Ctrl+C to stop)" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Seek to end for real-time monitoring
        logFile.seekg(0, std::ios::end);

        std::cout << COLOR_GREEN << "[Monitor] Successfully started monitoring" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Real-time monitoring loop (tail -f style)
        std::string line;
        while (true) {
            if (std::getline(logFile, line)) {
                if (!line.empty()) {
                    eventCount++;

                    // Format line as JSON
                    std::string jsonLog = formatSyslogLineAsJson(line, sourceName);

                    // Get current timestamp
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                    // Print entry with color
                    std::cout << COLOR_MAGENTA << "[Entry #" << eventCount
                             << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
                    std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
                    std::cout << "\n";
                }
            } else {
                // Clear EOF flag and wait
                logFile.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    } else {
        std::cout << COLOR_GREEN << "[Monitor] Mode: HISTORICAL" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "[Monitor] Reading historical entries..." << COLOR_RESET << std::endl;
        std::cout << "\n";

        std::cout << COLOR_GREEN << "[Monitor] Successfully started reading" << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
        std::cout << "\n";

        // Read all lines from file
        std::string line;
        while (std::getline(logFile, line)) {
            if (!line.empty()) {
                eventCount++;

                // Format line as JSON
                std::string jsonLog = formatSyslogLineAsJson(line, sourceName);

                // Get current timestamp
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

                // Print entry with color
                std::cout << COLOR_MAGENTA << "[Entry #" << eventCount
                         << " | +" << elapsed << "s]" << COLOR_RESET << std::endl;
                std::cout << COLOR_BLUE << jsonLog << COLOR_RESET << std::endl;
                std::cout << "\n";
            }
        }

        std::cout << COLOR_GREEN << "[Monitor] Finished reading historical entries" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "[Monitor] Total entries read: " << eventCount << COLOR_RESET << std::endl;
    }

    logFile.close();

    std::cout << "\n";
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "[Monitor] Monitoring session complete" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    printBanner();

    // Check for help flag
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h" || arg1 == "help") {
            printUsage();
            return 0;
        }
    }

    // Default configuration
    LogQueryConfig config;  // Defaults to REALTIME mode, SYSTEMD_JOURNAL source

    // Parse source argument
    if (argc >= 2) {
        std::string sourceStr = argv[1];
        std::transform(sourceStr.begin(), sourceStr.end(), sourceStr.begin(), ::tolower);

        if (sourceStr == "journal" || sourceStr == "systemd") {
            config.source = LogSource::SYSTEMD_JOURNAL;
        } else if (sourceStr == "syslog") {
            config.source = LogSource::SYSLOG_FILE;
        } else if (sourceStr == "auth") {
            config.source = LogSource::AUTH_LOG_FILE;
        } else if (sourceStr == "kern" || sourceStr == "kernel") {
            config.source = LogSource::KERN_LOG_FILE;
        } else {
            std::cerr << COLOR_RED << "[ERROR] Invalid source: " << sourceStr << COLOR_RESET << std::endl;
            printUsage();
            return 1;
        }
    }

    // Parse mode argument
    if (argc >= 3) {
        std::string modeStr = argv[2];
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

        if (modeStr == "realtime" || modeStr == "rt") {
            config.mode = LogReadMode::REALTIME;
        } else if (modeStr == "all" || modeStr == "historical") {
            config.mode = LogReadMode::HISTORICAL_ALL;
        } else if (modeStr == "recent") {
            config.mode = LogReadMode::HISTORICAL_RECENT;
            config.hoursBack = 24;  // Default
            if (argc >= 4) {
                config.hoursBack = std::atoi(argv[3]);
            }
        } else {
            std::cerr << COLOR_RED << "[ERROR] Invalid mode: " << modeStr << COLOR_RESET << std::endl;
            printUsage();
            return 1;
        }
    }

    // Start monitoring based on source
    if (config.source == LogSource::SYSTEMD_JOURNAL) {
        monitorJournalToConsole(config);
    } else {
        monitorLogFileToConsole(config);
    }

    return 0;
}
