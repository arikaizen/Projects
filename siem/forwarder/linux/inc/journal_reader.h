/**
 * @file journal_reader.h
 * @brief Linux system log reading and monitoring API
 *
 * This module provides functionality to read from various Linux log sources:
 * - systemd journal (modern Linux systems)
 * - Traditional syslog files (/var/log/syslog, /var/log/auth.log, etc.)
 * - Custom log files
 *
 * Supports both real-time monitoring and historical log reading.
 */

#ifndef JOURNAL_READER_H
#define JOURNAL_READER_H

#include <string>
#include <vector>
#include <systemd/sd-journal.h>

/**
 * @enum LogReadMode
 * @brief Defines how logs should be read from Linux system logs
 */
enum class LogReadMode {
    REALTIME,           ///< Monitor future log entries in real-time (default)
    HISTORICAL_ALL,     ///< Read all historical log entries from oldest to newest
    HISTORICAL_RECENT,  ///< Read recent log entries (last N hours)
    HISTORICAL_RANGE    ///< Read logs within a specific time range
};

/**
 * @enum LogSource
 * @brief Defines the source of log data to read
 */
enum class LogSource {
    SYSTEMD_JOURNAL,    ///< Read from systemd journal (default for modern Linux)
    SYSLOG_FILE,        ///< Read from /var/log/syslog
    AUTH_LOG_FILE,      ///< Read from /var/log/auth.log or /var/log/secure
    KERN_LOG_FILE,      ///< Read from /var/log/kern.log
    CUSTOM_FILE         ///< Read from custom log file path
};

/**
 * @struct LogQueryConfig
 * @brief Configuration for log queries
 */
struct LogQueryConfig {
    LogReadMode mode;           ///< Reading mode
    LogSource source;           ///< Log source to read from
    int hoursBack;              ///< Hours to look back (for HISTORICAL_RECENT)
    std::string startTime;      ///< Start time in ISO 8601 format (for HISTORICAL_RANGE)
    std::string endTime;        ///< End time in ISO 8601 format (for HISTORICAL_RANGE)
    std::string customPath;     ///< Custom log file path (for CUSTOM_FILE)
    std::vector<std::string> units;  ///< Filter by systemd units (e.g., "sshd.service")
    int minPriority;            ///< Minimum priority level (0=emerg, 7=debug, -1=all)

    /**
     * @brief Constructor with default values for real-time mode
     */
    LogQueryConfig()
        : mode(LogReadMode::REALTIME),
          source(LogSource::SYSTEMD_JOURNAL),
          hoursBack(24),
          startTime(""),
          endTime(""),
          customPath(""),
          minPriority(-1) {}
};

/**
 * @brief Format a journal entry as JSON
 *
 * Extracts all relevant properties from a systemd journal entry and formats them
 * into a JSON string suitable for transmission to the SIEM server.
 *
 * @param journal Pointer to the systemd journal
 * @return JSON-formatted string containing log data
 */
std::string formatJournalEntryAsJson(sd_journal* journal);

/**
 * @brief Format a syslog line as JSON
 *
 * Parses a traditional syslog line and formats it as JSON.
 * Supports standard syslog format: "timestamp hostname process[pid]: message"
 *
 * @param logLine Raw syslog line
 * @param source Log source identifier (e.g., "syslog", "auth.log")
 * @return JSON-formatted string containing log data
 */
std::string formatSyslogLineAsJson(const std::string& logLine, const std::string& source);

/**
 * @brief Get log file path for a given log source
 *
 * Returns the standard file path for common log sources on Linux.
 * Handles distribution differences (e.g., auth.log vs secure).
 *
 * @param source Log source type
 * @return Full path to log file, or empty string if not applicable
 */
std::string getLogFilePath(LogSource source);

/**
 * @brief Get current time in seconds since epoch
 *
 * Helper function to get current system time with optional offset.
 *
 * @param hoursOffset Hours to offset from current time (negative = past, positive = future)
 * @return Time in seconds since Unix epoch
 */
uint64_t getTimestamp(int hoursOffset = 0);

#endif // JOURNAL_READER_H
