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

// wevtapi.lib: EvtRender, EvtSubscribe, EvtNext, EvtClose
#pragma comment(lib, "wevtapi.lib")
/** 
 * DWORD - unsigned log
 * PEVT_VARIANT - pointer to EVT_VARIANT structure tagged union type that can hold many 
   different value kinds (string, numbers, GUIDs, FILETIME, etc.).
 
    typedef struct _EVT_VARIANT {
    union {
        ULONGLONG FileTimeVal;      // for EvtVarTypeFileTime
        HANDLE    HandleVal;        // for EvtVarTypeHandle
        UINT32    UInt32Val;        // for EvtVarTypeUInt32
        UINT64    UInt64Val;        // for EvtVarTypeUInt64
        INT32     Int32Val;         // for EvtVarTypeInt32
        INT64     Int64Val;         // for EvtVarTypeInt64
        FLOAT     FloatVal;         // for EvtVarTypeSingle
        DOUBLE    DoubleVal;        // for EvtVarTypeDouble
        BOOL      BoolVal;          // for EvtVarTypeBoolean
        LPWSTR    StringVal;        // for EvtVarTypeString (wide string)
        LPSTR     AnsiStringVal;    // for EvtVarTypeAnsiString
        LPBYTE    BinaryVal;        // for EvtVarTypeBinary
        GUID*     GuidVal;          // for EvtVarTypeGuid
        size_t    SizeTVal;         // for EvtVarTypeSizeT
        WCHAR*    XmlVal;           // for EvtVarTypeXml
        LPWSTR*   StringArrayVal;   // for array types
    };
    DWORD PropertyLength;         // length for array/binary types
    DWORD Type;                   // discriminant: tells you which field is valid
    DWORD Reserved;               // reserved for future use
    } EVT_VARIANT, *PEVT_VARIANT;


 * 
 * 
 *
 */
std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = nullptr;
    std::string result = "";

    // Create render context for system properties
    EVT_HANDLE hContext = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
    if (hContext == NULL) {
        return "";  // Failed to create context
    }

    // First call to get required buffer size
    if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

            if (nullptr != pRenderedValues) {
                // Second call with allocated buffer
                if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                             pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                    // Second call also failed - cleanup and return empty
                    delete[](BYTE*)pRenderedValues;
                    EvtClose(hContext);
                    return "";
                }
            }
        } else {
            // Unexpected error
            EvtClose(hContext);
            return "";
        }
    }

    // Extract the requested property
    if (pRenderedValues && propertyId < dwPropertyCount) {
        PEVT_VARIANT pProperty = &pRenderedValues[propertyId];

        // Convert property value to string based on type
        switch (pProperty->Type) {
            case EvtVarTypeString:
                if (pProperty->StringVal) {
                    // String type - convert from wide char to UTF-8
                    int size = WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, nullptr, 0, nullptr, nullptr);
                    if (size > 0) {
                        char* buffer = new char[size];
                        WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, buffer, size, nullptr, nullptr);
                        result = buffer;
                        delete[] buffer;
                    }
                }
                break;

            case EvtVarTypeByte:
                result = std::to_string(pProperty->ByteVal);
                break;

            case EvtVarTypeSByte:
                result = std::to_string(pProperty->SByteVal);
                break;

            case EvtVarTypeInt16:
                result = std::to_string(pProperty->Int16Val);
                break;

            case EvtVarTypeUInt16:
                result = std::to_string(pProperty->UInt16Val);
                break;

            case EvtVarTypeInt32:
                result = std::to_string(pProperty->Int32Val);
                break;

            case EvtVarTypeUInt32:
                result = std::to_string(pProperty->UInt32Val);
                break;

            case EvtVarTypeInt64:
                result = std::to_string(pProperty->Int64Val);
                break;

            case EvtVarTypeUInt64:
                result = std::to_string(pProperty->UInt64Val);
                break;

            case EvtVarTypeBoolean:
                result = pProperty->BooleanVal ? "true" : "false";
                break;

            case EvtVarTypeFileTime:
                // FileTime is a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601
                result = std::to_string(pProperty->FileTimeVal);
                break;

            default:
                // Unsupported type - return empty string
                result = "";
                break;
        }
    }

    // Cleanup
    if (pRenderedValues) {
        delete[](BYTE*)pRenderedValues;
    }
    if (hContext) {
        EvtClose(hContext);
    }

    return result;
}

std::string getRawEventXml(EVT_HANDLE hEvent) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    LPWSTR pRenderedContent = nullptr;
    std::string result = "";

    // First call to get required buffer size
    if (!EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize,
                   pRenderedContent, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedContent = (LPWSTR)malloc(dwBufferSize);

            if (pRenderedContent != nullptr) {
                // Second call with allocated buffer
                if (EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize,
                             pRenderedContent, &dwBufferUsed, &dwPropertyCount)) {
                    // Convert wide string to UTF-8
                    int size = WideCharToMultiByte(CP_UTF8, 0, pRenderedContent, -1,
                                                   nullptr, 0, nullptr, nullptr);
                    if (size > 0) {
                        char* buffer = new char[size];
                        WideCharToMultiByte(CP_UTF8, 0, pRenderedContent, -1,
                                          buffer, size, nullptr, nullptr);
                        result = buffer;
                        delete[] buffer;
                    }
                } else {
                    // Second call failed
                    free(pRenderedContent);
                    return "";
                }
                free(pRenderedContent);
            }
        } else {
            // Unexpected error
            return "";
        }
    }

    return result;
}

std::string getEventMessage(EVT_HANDLE hEvent) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    LPWSTR pMessage = nullptr;
    std::string result = "";

    // Get the publisher metadata (required for EvtFormatMessage)
    std::wstring providerNameW;
    std::string providerName = getEventProperty(hEvent, EvtSystemProviderName);

    if (!providerName.empty()) {
        // Convert provider name to wide string
        int size = MultiByteToWideChar(CP_UTF8, 0, providerName.c_str(), -1, nullptr, 0);
        if (size > 0) {
            wchar_t* wbuffer = new wchar_t[size];
            MultiByteToWideChar(CP_UTF8, 0, providerName.c_str(), -1, wbuffer, size);
            providerNameW = wbuffer;
            delete[] wbuffer;
        }
    }

    // Open publisher metadata
    EVT_HANDLE hPublisher = NULL;
    if (!providerNameW.empty()) {
        hPublisher = EvtOpenPublisherMetadata(NULL, providerNameW.c_str(), NULL, 0, 0);
    }

    // First call to get required buffer size
    if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent,
                         dwBufferSize, pMessage, &dwBufferUsed)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pMessage = (LPWSTR)malloc(dwBufferSize * sizeof(WCHAR));

            if (pMessage != nullptr) {
                // Second call with allocated buffer
                if (EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent,
                                   dwBufferSize, pMessage, &dwBufferUsed)) {
                    // Convert wide string to UTF-8
                    int size = WideCharToMultiByte(CP_UTF8, 0, pMessage, -1, nullptr, 0, nullptr, nullptr);
                    if (size > 0) {
                        char* buffer = new char[size];
                        WideCharToMultiByte(CP_UTF8, 0, pMessage, -1, buffer, size, nullptr, nullptr);
                        result = buffer;
                        delete[] buffer;
                    }
                }
                free(pMessage);
            }
        }
    }

    if (hPublisher) {
        EvtClose(hPublisher);
    }

    return result;
}

std::string formatEventAsPlainText(EVT_HANDLE hEvent) {
    std::ostringstream text;

    // Extract event properties
    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    std::string provider = getEventProperty(hEvent, EvtSystemProviderName);

    // Extract timestamp
    ULONGLONG timestamp = 0;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = nullptr;

    EVT_HANDLE hContext = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
    if (hContext != NULL) {
        if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                       pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                dwBufferSize = dwBufferUsed;
                pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

                if (pRenderedValues) {
                    if (EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                                pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                        if (dwPropertyCount > EvtSystemTimeCreated) {
                            timestamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
                        }
                    }
                    delete[](BYTE*)pRenderedValues;
                }
            }
        }
        EvtClose(hContext);
    }

    // Convert timestamp to readable format
    std::string timeStr = "Unknown";
    if (timestamp != 0) {
        FILETIME ft;
        ft.dwLowDateTime = (DWORD)(timestamp & 0xFFFFFFFF);
        ft.dwHighDateTime = (DWORD)(timestamp >> 32);

        SYSTEMTIME st;
        if (FileTimeToSystemTime(&ft, &st)) {
            char buffer[64];
            sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond);
            timeStr = buffer;
        }
    }

    // Convert level to text
    std::string levelStr = "Unknown";
    if (!level.empty()) {
        int levelNum = std::stoi(level);
        switch (levelNum) {
            case 1: levelStr = "Critical"; break;
            case 2: levelStr = "Error"; break;
            case 3: levelStr = "Warning"; break;
            case 4: levelStr = "Information"; break;
            case 5: levelStr = "Verbose"; break;
            default: levelStr = "Level " + level; break;
        }
    }

    // Get event message
    std::string message = getEventMessage(hEvent);

    // Format as plain text
    text << "========================================" << std::endl;
    text << "Event ID:    " << eventId << std::endl;
    text << "Level:       " << levelStr << std::endl;
    text << "Time:        " << timeStr << std::endl;
    text << "Channel:     " << channel << std::endl;
    text << "Computer:    " << computer << std::endl;
    text << "Provider:    " << provider << std::endl;
    if (!message.empty()) {
        text << "Message:     " << message << std::endl;
    }
    text << "========================================";

    return text.str();
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
    PEVT_VARIANT pRenderedValues = nullptr;

    // Create render context for system properties
    EVT_HANDLE hContext = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
    if (hContext != NULL) {
        // First call to get required buffer size
        if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                       pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
            if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
                dwBufferSize = dwBufferUsed;
                pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

                if (pRenderedValues) {
                    // Second call with allocated buffer
                    if (EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize,
                                pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                        // Get timestamp from TimeCreated field (only if render succeeded)
                        if (dwPropertyCount > EvtSystemTimeCreated) {
                            timestamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
                        }
                    }

                    delete[](BYTE*)pRenderedValues;
                }
            }
        }
        EvtClose(hContext);
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
