/**
 * @file journal_reader.cpp
 * @brief Implementation of systemd journal reading functions
 *
 * Provides functionality to read and parse systemd journal entries.
 */

#include "journal_reader.h"
#include "json_utils.h"
#include <sstream>
#include <ctime>

std::string formatJournalEntryAsJson(sd_journal* journal) {
    std::ostringstream json;

    // Extract common journal fields
    const char* message = nullptr;
    const char* priority = nullptr;
    const char* unit = nullptr;
    const char* hostname = nullptr;
    const char* pid = nullptr;
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
    json << "\"timestamp\":" << timestamp;
    json << "}";

    return json.str();
}
