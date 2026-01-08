/**
 * @file event_log_reader.h
 * @brief Windows Event Log reading and monitoring API
 *
 * This module provides functionality to subscribe to Windows Event Log channels,
 * read events in real-time or query historical events, and extract event properties.
 */

#ifndef EVENT_LOG_READER_H
#define EVENT_LOG_READER_H

#include <windows.h>
#include <winevt.h>
#include <string>

/**
 * @enum EventReadMode
 * @brief Defines how events should be read from Windows Event Log
 */
enum class EventReadMode {
    REALTIME,       ///< Monitor future events in real-time (default)
    HISTORICAL_ALL, ///< Read all historical events from oldest to newest
    HISTORICAL_RECENT, ///< Read recent historical events (last N hours)
    HISTORICAL_RANGE  ///< Read events within a specific time range
};

/**
 * @struct EventQueryConfig
 * @brief Configuration for event log queries
 */
struct EventQueryConfig {
    EventReadMode mode;           ///< Reading mode
    int hoursBack;                ///< Hours to look back (for HISTORICAL_RECENT)
    std::wstring startTime;       ///< Start time in ISO 8601 format (for HISTORICAL_RANGE)
    std::wstring endTime;         ///< End time in ISO 8601 format (for HISTORICAL_RANGE)

    /**
     * @brief Constructor with default values for real-time mode
     */
    EventQueryConfig()
        : mode(EventReadMode::REALTIME),
          hoursBack(24),
          startTime(L""),
          endTime(L"") {}
};

/**
 * @brief Extract a specific property from a Windows Event Log event
 *
 * Queries the Windows Event Log API to retrieve a specific system property
 * from an event (e.g., Event ID, Level, Channel, Computer name).
 *
 * @param hEvent Handle to the Windows Event Log event
 * @param propertyId System property identifier to retrieve
 * @return String representation of the property value
 */
std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId);

/**
 * @brief Format a Windows Event Log event as JSON
 *
 * Extracts all relevant properties from a Windows Event and formats them
 * into a JSON string suitable for transmission to the SIEM server.
 *
 * @param hEvent Handle to the Windows Event Log event
 * @return JSON-formatted string containing event data
 */
std::string formatEventAsJson(EVT_HANDLE hEvent);

/**
 * @brief Build XPath query for historical event filtering
 *
 * Creates an XPath query string based on the query configuration
 * to filter events by time range.
 *
 * @param config Query configuration with time parameters
 * @return XPath query string for use with EvtQuery
 */
std::wstring buildHistoricalQuery(const EventQueryConfig& config);

/**
 * @brief Get current time in Windows FILETIME format as string
 *
 * Helper function to get the current system time formatted
 * for use in XPath queries.
 *
 * @param hoursOffset Hours to offset from current time (negative = past, positive = future)
 * @return Time string in format suitable for XPath SystemTime attribute
 */
std::wstring getTimeString(int hoursOffset = 0);

#endif // EVENT_LOG_READER_H
