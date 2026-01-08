/**
 * @file event_log_reader.cpp
 * @brief Implementation of Windows Event Log reading functions
 *
 * Provides functionality to read and parse Windows Event Log events
 * in both real-time and historical modes.
 */

#include "event_log_reader.h"
#include "json_utils.h"
#include <sstream>
#include <iomanip>
#include <ctime>

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

std::wstring getTimeString(int hoursOffset) {
    // Get current system time
    SYSTEMTIME st;
    GetSystemTime(&st);

    // Convert to FILETIME
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);

    // Convert to 64-bit integer (100-nanosecond intervals since 1601)
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    // Apply hours offset (1 hour = 10,000,000 * 3600 = 36,000,000,000 intervals)
    LONGLONG offset = (LONGLONG)hoursOffset * 36000000000LL;
    uli.QuadPart += offset;

    // Convert back to FILETIME
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;

    // Convert to SYSTEMTIME
    FileTimeToSystemTime(&ft, &st);

    // Format as ISO 8601 string (YYYY-MM-DDTHH:MM:SS.mmmZ)
    std::wostringstream oss;
    oss << std::setfill(L'0')
        << std::setw(4) << st.wYear << L"-"
        << std::setw(2) << st.wMonth << L"-"
        << std::setw(2) << st.wDay << L"T"
        << std::setw(2) << st.wHour << L":"
        << std::setw(2) << st.wMinute << L":"
        << std::setw(2) << st.wSecond << L"."
        << std::setw(3) << st.wMilliseconds << L"Z";

    return oss.str();
}

std::wstring buildHistoricalQuery(const EventQueryConfig& config) {
    std::wostringstream query;

    switch (config.mode) {
        case EventReadMode::REALTIME:
            // For real-time, use wildcard (no time filtering)
            query << L"*";
            break;

        case EventReadMode::HISTORICAL_ALL:
            // Query all events (no time filtering)
            query << L"*";
            break;

        case EventReadMode::HISTORICAL_RECENT: {
            // Query events from the last N hours
            std::wstring startTime = getTimeString(-config.hoursBack);
            query << L"*[System[TimeCreated[@SystemTime>='" << startTime << L"']]]";
            break;
        }

        case EventReadMode::HISTORICAL_RANGE:
            // Query events within specific time range
            if (!config.startTime.empty() && !config.endTime.empty()) {
                query << L"*[System[TimeCreated[@SystemTime>='" << config.startTime
                      << L"' and @SystemTime<='" << config.endTime << L"']]]";
            } else if (!config.startTime.empty()) {
                query << L"*[System[TimeCreated[@SystemTime>='" << config.startTime << L"']]]";
            } else if (!config.endTime.empty()) {
                query << L"*[System[TimeCreated[@SystemTime<='" << config.endTime << L"']]]";
            } else {
                // No time range specified, query all
                query << L"*";
            }
            break;

        default:
            query << L"*";
            break;
    }

    return query.str();
}
