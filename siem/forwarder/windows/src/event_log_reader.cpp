#include "event_log_reader.h"
#include <iostream>
#include <sstream>
#include <iomanip>

EventLogReader::EventLogReader(const std::string& channel)
    : channelName(channel), hSubscription(NULL), initialized(false) {
}

EventLogReader::~EventLogReader() {
    close();
}

bool EventLogReader::initialize() {
    if (initialized) {
        return true;
    }

    // Open the event log channel
    std::wstring wChannelName(channelName.begin(), channelName.end());

    initialized = true;
    return true;
}

std::vector<EventData> EventLogReader::readEvents(int maxEvents) {
    std::vector<EventData> events;

    if (!initialized) {
        std::cerr << "EventLogReader not initialized" << std::endl;
        return events;
    }

    std::wstring wChannelName(channelName.begin(), channelName.end());

    // Query for events
    EVT_HANDLE hResults = EvtQuery(
        NULL,
        wChannelName.c_str(),
        NULL,
        EvtQueryChannelPath | EvtQueryReverseDirection
    );

    if (hResults == NULL) {
        std::cerr << "EvtQuery failed with error: " << GetLastError() << std::endl;
        return events;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[10];

    while (events.size() < maxEvents) {
        if (!EvtNext(hResults, 10, hEvents, INFINITE, 0, &dwReturned)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) {
                std::cerr << "EvtNext failed: " << GetLastError() << std::endl;
            }
            break;
        }

        for (DWORD i = 0; i < dwReturned && events.size() < maxEvents; i++) {
            EventData event;

            // Get event properties
            event.eventId = getEventProperty(hEvents[i], EvtSystemEventID);
            event.level = getEventProperty(hEvents[i], EvtSystemLevel);
            event.source = getEventProperty(hEvents[i], EvtSystemProviderName);
            event.computer = getEventProperty(hEvents[i], EvtSystemComputer);
            event.timestamp = getEventProperty(hEvents[i], EvtSystemTimeCreated);
            event.message = renderEventXml(hEvents[i]);

            events.push_back(event);
            EvtClose(hEvents[i]);
        }
    }

    EvtClose(hResults);
    return events;
}

std::string EventLogReader::getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;

    // Get required buffer size
    EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);

    if (dwBufferUsed == 0) {
        return "";
    }

    std::vector<BYTE> buffer(dwBufferUsed);

    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferUsed, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
        return "";
    }

    PEVT_VARIANT pRenderedValues = (PEVT_VARIANT)buffer.data();

    if (propertyId < dwPropertyCount) {
        EVT_VARIANT& value = pRenderedValues[propertyId];

        std::ostringstream oss;
        switch (value.Type) {
            case EvtVarTypeString:
                if (value.StringVal) {
                    std::wstring ws(value.StringVal);
                    return std::string(ws.begin(), ws.end());
                }
                break;
            case EvtVarTypeUInt16:
                oss << value.UInt16Val;
                return oss.str();
            case EvtVarTypeUInt32:
                oss << value.UInt32Val;
                return oss.str();
            case EvtVarTypeFileTime:
                oss << value.FileTimeVal;
                return oss.str();
        }
    }

    return "";
}

std::string EventLogReader::renderEventXml(EVT_HANDLE hEvent) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;

    EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);

    if (dwBufferUsed == 0) {
        return "";
    }

    std::vector<WCHAR> buffer(dwBufferUsed / sizeof(WCHAR));

    if (EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferUsed, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
        std::wstring ws(buffer.data());
        return std::string(ws.begin(), ws.end());
    }

    return "";
}

void EventLogReader::close() {
    if (hSubscription != NULL) {
        EvtClose(hSubscription);
        hSubscription = NULL;
    }
    initialized = false;

/**
 * @file event_log_reader.cpp
 * @brief Implementation of Windows Event Log reading functions
 *
 * Provides functionality to read and parse Windows Event Log events.
 */

#include "event_log_reader.h"
#include "json_utils.h"
#include <sstream>

#pragma comment(lib, "wevtapi.lib")

std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;
    std::string result = "";

    // First call to get required buffer size
    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);

            if (pRenderedValues) {
                // Second call with allocated buffer
                EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize,
                         pRenderedValues, &dwBufferUsed, &dwPropertyCount);
            }
        }
    }

    // Extract the requested property
    if (pRenderedValues && propertyId < dwPropertyCount) {
        PEVT_VARIANT pProperty = &pRenderedValues[propertyId];

        // Convert property value to string based on type
        if (pProperty->Type == EvtVarTypeString && pProperty->StringVal) {
            // String type - convert from wide char to UTF-8
            int size = WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, NULL, 0, NULL, NULL);
            if (size > 0) {
                char* buffer = new char[size];
                WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, buffer, size, NULL, NULL);
                result = buffer;
                delete[] buffer;
            }
        } else if (pProperty->Type == EvtVarTypeUInt16) {
            result = std::to_string(pProperty->UInt16Val);
        } else if (pProperty->Type == EvtVarTypeUInt32) {
            result = std::to_string(pProperty->UInt32Val);
        } else if (pProperty->Type == EvtVarTypeUInt64) {
            result = std::to_string(pProperty->UInt64Val);
        }
    }

    // Cleanup
    if (pRenderedValues) {
        free(pRenderedValues);
    }

    return result;
}

std::string formatEventAsJson(EVT_HANDLE hEvent) {
    std::ostringstream json;

    // Extract event properties
    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);

    // Extract timestamp
    ULONGLONG timestamp = 0;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;

    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);

            if (pRenderedValues) {
                EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize,
                         pRenderedValues, &dwBufferUsed, &dwPropertyCount);

                // Get timestamp from TimeCreated field
                if (dwPropertyCount > EvtSystemTimeCreated) {
                    timestamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
                }

                free(pRenderedValues);
            }
        }
    }

    // Build JSON object
    json << "{";
    json << "\"event_id\":\"" << escapeJson(eventId) << "\",";
    json << "\"level\":\"" << escapeJson(level) << "\",";
    json << "\"channel\":\"" << escapeJson(channel) << "\",";
    json << "\"computer\":\"" << escapeJson(computer) << "\",";
    json << "\"timestamp\":" << timestamp;
    json << "}";

    return json.str();
}
