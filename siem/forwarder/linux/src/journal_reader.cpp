/**
 * @file journal_reader.cpp
 * @brief Implementation of Linux system log reading functions
 *
 * Provides functionality to read and parse system logs from various sources
 * including systemd journal and traditional syslog files.
 */

#include "journal_reader.h"
#include "json_utils.h"
#include <sstream>
#include <ctime>
#include <regex>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

std::string formatJournalEntryAsJson(sd_journal* journal) {
    std::ostringstream json;

    // Extract common journal fields
    const char* message = nullptr;
    const char* priority = nullptr;
    const char* unit = nullptr;
    const char* hostname = nullptr;
    const char* pid = nullptr;
    const char* comm = nullptr;
    size_t length;

    // Get MESSAGE field
    if (sd_journal_get_data(journal, "MESSAGE", (const void**)&message, &length) >= 0) {
        message += 8;  // Skip "MESSAGE=" prefix
    } else {
        message = "";
    }

    // Get PRIORITY field (0-7, syslog priority)
    if (sd_journal_get_data(journal, "PRIORITY", (const void**)&priority, &length) >= 0) {
        priority += 9;  // Skip "PRIORITY=" prefix
    } else {
        priority = "6";  // Default to INFO
    }

    // Get _SYSTEMD_UNIT or SYSLOG_IDENTIFIER
    if (sd_journal_get_data(journal, "_SYSTEMD_UNIT", (const void**)&unit, &length) >= 0) {
        unit += 14;  // Skip "_SYSTEMD_UNIT=" prefix
    } else if (sd_journal_get_data(journal, "SYSLOG_IDENTIFIER", (const void**)&unit, &length) >= 0) {
        unit += 18;  // Skip "SYSLOG_IDENTIFIER=" prefix
    } else {
        unit = "system";
    }

    // Get _HOSTNAME
    if (sd_journal_get_data(journal, "_HOSTNAME", (const void**)&hostname, &length) >= 0) {
        hostname += 10;  // Skip "_HOSTNAME=" prefix
    } else {
        hostname = "localhost";
    }

    // Get _PID
    if (sd_journal_get_data(journal, "_PID", (const void**)&pid, &length) >= 0) {
        pid += 5;  // Skip "_PID=" prefix
    } else {
        pid = "0";
    }

    // Get _COMM (command name)
    if (sd_journal_get_data(journal, "_COMM", (const void**)&comm, &length) >= 0) {
        comm += 6;  // Skip "_COMM=" prefix
    } else {
        comm = "";
    }

    // Get timestamp
    uint64_t timestamp;
    if (sd_journal_get_realtime_usec(journal, &timestamp) < 0) {
        timestamp = 0;
    }

    // Build JSON object
    json << "{";
    json << "\"message\":\"" << escapeJson(std::string(message)) << "\",";
    json << "\"priority\":\"" << std::string(priority) << "\",";
    json << "\"unit\":\"" << escapeJson(std::string(unit)) << "\",";
    json << "\"hostname\":\"" << escapeJson(std::string(hostname)) << "\",";
    json << "\"pid\":\"" << std::string(pid) << "\",";
    json << "\"comm\":\"" << escapeJson(std::string(comm)) << "\",";
    json << "\"timestamp\":" << timestamp << ",";
    json << "\"source\":\"systemd-journal\"";
    json << "}";

    return json.str();
}

std::string formatSyslogLineAsJson(const std::string& logLine, const std::string& source) {
    std::ostringstream json;

    // Parse syslog format: "timestamp hostname process[pid]: message"
    // Example: "Jan 14 10:30:45 myhost sshd[1234]: Connection from 192.168.1.1"

    std::string timestamp, hostname, process, pid, message;
    std::regex syslogPattern(R"(^(\w+\s+\d+\s+\d+:\d+:\d+)\s+(\S+)\s+([^\[:\s]+)(?:\[(\d+)\])?:\s*(.*)$)");
    std::smatch matches;

    if (std::regex_search(logLine, matches, syslogPattern)) {
        timestamp = matches[1].str();
        hostname = matches[2].str();
        process = matches[3].str();
        pid = matches[4].matched ? matches[4].str() : "0";
        message = matches[5].str();
    } else {
        // Fallback: treat entire line as message
        message = logLine;
        hostname = "localhost";
        process = "unknown";
        pid = "0";

        // Get current timestamp
        auto now = std::time(nullptr);
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M:%S", std::localtime(&now));
        timestamp = timeBuf;
    }

    // Convert timestamp to microseconds since epoch
    uint64_t timestamp_usec = static_cast<uint64_t>(std::time(nullptr)) * 1000000;

    // Build JSON object
    json << "{";
    json << "\"message\":\"" << escapeJson(message) << "\",";
    json << "\"priority\":\"6\",";  // Default to INFO
    json << "\"unit\":\"" << escapeJson(process) << "\",";
    json << "\"hostname\":\"" << escapeJson(hostname) << "\",";
    json << "\"pid\":\"" << pid << "\",";
    json << "\"comm\":\"" << escapeJson(process) << "\",";
    json << "\"timestamp\":" << timestamp_usec << ",";
    json << "\"source\":\"" << escapeJson(source) << "\"";
    json << "}";

    return json.str();
}

std::string getLogFilePath(LogSource source) {
    switch (source) {
        case LogSource::SYSLOG_FILE:
            // Try Debian/Ubuntu style first
            if (access("/var/log/syslog", F_OK) == 0) {
                return "/var/log/syslog";
            }
            // Fall back to RHEL/CentOS style
            return "/var/log/messages";

        case LogSource::AUTH_LOG_FILE:
            // Try Debian/Ubuntu style first
            if (access("/var/log/auth.log", F_OK) == 0) {
                return "/var/log/auth.log";
            }
            // Fall back to RHEL/CentOS style
            return "/var/log/secure";

        case LogSource::KERN_LOG_FILE:
            return "/var/log/kern.log";

        case LogSource::SYSTEMD_JOURNAL:
        default:
            return "";  // Not a file-based source
    }
}

uint64_t getTimestamp(int hoursOffset) {
    auto now = std::time(nullptr);
    auto offset_time = now + (hoursOffset * 3600);
    return static_cast<uint64_t>(offset_time);
}
