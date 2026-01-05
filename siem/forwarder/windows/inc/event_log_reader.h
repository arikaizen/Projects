#ifndef EVENT_LOG_READER_H
#define EVENT_LOG_READER_H

#include <string>
#include <vector>
#include <memory>

// Windows headers must be included in specific order
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winevt.h>

// Link with wevtapi.lib
#pragma comment(lib, "wevtapi.lib")

struct EventData {
    std::string timestamp;
    std::string eventId;
    std::string level;
    std::string source;
    std::string message;
    std::string computer;
};

class EventLogReader {
public:
    EventLogReader(const std::string& channel = "Application");
    ~EventLogReader();

    bool initialize();
    std::vector<EventData> readEvents(int maxEvents = 100);
    void close();

private:
    std::string channelName;
    EVT_HANDLE hSubscription;
    bool initialized;

    std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId);
    std::string renderEventXml(EVT_HANDLE hEvent);
};

/**
 * @file event_log_reader.h
 * @brief Windows Event Log reading and monitoring API
 *
 * This module provides functionality to subscribe to Windows Event Log channels,
 * read events in real-time, and extract event properties.
 */

#ifndef EVENT_LOG_READER_H
#define EVENT_LOG_READER_H

#include <windows.h>
#include <winevt.h>
#include <string>

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

#endif // EVENT_LOG_READER_H
