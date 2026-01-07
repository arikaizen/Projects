/**
 * @file event_log_reader.cpp
 * @brief Implementation of Windows Event Log reading functions
 *
 * Provides functionality to read and parse Windows Event Log events.
 */

#include "event_log_reader.h" // EvtRender/EvtNext wrappers (defs)
#include "json_utils.h"       // escapeJson
#include <sstream>             // std::ostringstream

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

    // First call to get required buffer size
    // EvtRender: Extract event data into a buffer of EVT_VARIANT structures. Returns TRUE on success, FALSE on failure.
    if (!EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            // Use new[] for buffer allocation (newer C++ style instead of malloc)
            pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

            if (nullptr != pRenderedValues) {
                // Second call with allocated buffer
                EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
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
            // WideCharToMultiByte: Convert wide-character string to multibyte (UTF-8) string. Returns number of bytes written on success, 0 on failure.
            int size = WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                char* buffer = new char[size];
                // WideCharToMultiByte: Convert wide-character string to multibyte string. Returns number of bytes written on success, 0 on failure.
                WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, buffer, size, nullptr, nullptr);
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
        // Use delete[] to match new[] allocation (newer C++ style instead of free)
        delete[](BYTE*)pRenderedValues;
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
    PEVT_VARIANT pRenderedValues = nullptr;

    // EvtRender: Extract event data into a buffer of EVT_VARIANT structures. Returns TRUE on success, FALSE on failure.
    if (!EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        // GetLastError: Retrieve error code from last failed Windows API call (from <windows.h>). Returns platform-specific error code.
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            // Use new[] for buffer allocation (newer C++ style instead of malloc)
            pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

            if (pRenderedValues) {
                EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
                         pRenderedValues, &dwBufferUsed, &dwPropertyCount);

                // Get timestamp from TimeCreated field
                if (dwPropertyCount > EvtSystemTimeCreated) {
                    timestamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
                }

                // Use delete[] to match new[] allocation (newer C++ style instead of free)
                delete[](BYTE*)pRenderedValues;
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
