/**
 * @file event_log_reader.cpp
 * @brief Implementation of Windows Event Log reading functions
 *
 * Provides functionality to read and parse Windows Event Log events.
 */

#include "../inc/event_log_reader.h"
#include "../inc/json_utils.h"
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
