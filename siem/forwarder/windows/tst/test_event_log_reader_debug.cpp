/**
 * @file test_event_log_reader_debug.cpp
 * @brief Debug version of event log reader test with detailed diagnostics
 */

#include "../inc/event_log_reader.h"
#include <iostream>
#include <winevt.h>
#include <windows.h>

// Helper to get property type name
const char* getTypeName(DWORD type) {
    switch(type) {
        case EvtVarTypeNull: return "Null";
        case EvtVarTypeString: return "String";
        case EvtVarTypeAnsiString: return "AnsiString";
        case EvtVarTypeSByte: return "SByte";
        case EvtVarTypeByte: return "Byte";
        case EvtVarTypeInt16: return "Int16";
        case EvtVarTypeUInt16: return "UInt16";
        case EvtVarTypeInt32: return "Int32";
        case EvtVarTypeUInt32: return "UInt32";
        case EvtVarTypeInt64: return "Int64";
        case EvtVarTypeUInt64: return "UInt64";
        case EvtVarTypeSingle: return "Single";
        case EvtVarTypeDouble: return "Double";
        case EvtVarTypeBoolean: return "Boolean";
        case EvtVarTypeBinary: return "Binary";
        case EvtVarTypeGuid: return "Guid";
        case EvtVarTypeSizeT: return "SizeT";
        case EvtVarTypeFileTime: return "FileTime";
        case EvtVarTypeSysTime: return "SysTime";
        case EvtVarTypeSid: return "Sid";
        case EvtVarTypeHexInt32: return "HexInt32";
        case EvtVarTypeHexInt64: return "HexInt64";
        case EvtVarTypeEvtHandle: return "EvtHandle";
        case EvtVarTypeEvtXml: return "EvtXml";
        default: return "Unknown";
    }
}

void debugEventProperties() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Event Log Property Type Diagnostic" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    // Query a single event
    EVT_HANDLE hResults = EvtQuery(NULL, L"System", L"*",
                                   EvtQueryChannelPath | EvtQueryForwardDirection);

    if (hResults == NULL) {
        std::cout << "ERROR: Failed to query System log" << std::endl;
        std::cout << "Error code: " << GetLastError() << std::endl;
        return;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[1];

    if (!EvtNext(hResults, 1, hEvents, 5000, 0, &dwReturned) || dwReturned == 0) {
        std::cout << "ERROR: No events returned" << std::endl;
        std::cout << "Error code: " << GetLastError() << std::endl;
        EvtClose(hResults);
        return;
    }

    std::cout << "Successfully retrieved 1 event from System log" << std::endl << std::endl;

    EVT_HANDLE hEvent = hEvents[0];

    // Render event properties
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = nullptr;

    // First call to get buffer size
    if (!EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
                   pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            std::cout << "First EvtRender call: Need buffer size " << dwBufferUsed << " bytes" << std::endl;
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)(new BYTE[dwBufferSize]);

            if (pRenderedValues != nullptr) {
                if (EvtRender(nullptr, hEvent, EvtRenderEventValues, dwBufferSize,
                             pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                    std::cout << "Second EvtRender call: SUCCESS" << std::endl;
                    std::cout << "Property count: " << dwPropertyCount << std::endl << std::endl;
                } else {
                    std::cout << "Second EvtRender call: FAILED with error " << GetLastError() << std::endl;
                    delete[](BYTE*)pRenderedValues;
                    EvtClose(hEvent);
                    EvtClose(hResults);
                    return;
                }
            }
        } else {
            std::cout << "First EvtRender call: Unexpected error " << GetLastError() << std::endl;
            EvtClose(hEvent);
            EvtClose(hResults);
            return;
        }
    }

    // Display all property types
    std::cout << "Property Type Analysis:" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    const char* propNames[] = {
        "EvtSystemProviderName",
        "EvtSystemProviderGuid",
        "EvtSystemEventID",
        "EvtSystemQualifiers",
        "EvtSystemLevel",
        "EvtSystemTask",
        "EvtSystemOpcode",
        "EvtSystemKeywords",
        "EvtSystemTimeCreated",
        "EvtSystemEventRecordId",
        "EvtSystemActivityID",
        "EvtSystemRelatedActivityID",
        "EvtSystemProcessID",
        "EvtSystemThreadID",
        "EvtSystemChannel",
        "EvtSystemComputer",
        "EvtSystemUserID",
        "EvtSystemVersion"
    };

    for (DWORD i = 0; i < dwPropertyCount && i < 18; i++) {
        PEVT_VARIANT pProperty = &pRenderedValues[i];

        std::cout << "[" << i << "] " << propNames[i] << std::endl;
        std::cout << "    Type: " << getTypeName(pProperty->Type)
                  << " (code: " << pProperty->Type << ")" << std::endl;

        // Try to show the value
        if (pProperty->Type == EvtVarTypeString && pProperty->StringVal) {
            std::wcout << L"    Value: " << pProperty->StringVal << std::endl;
        } else if (pProperty->Type == EvtVarTypeUInt16) {
            std::cout << "    Value: " << pProperty->UInt16Val << std::endl;
        } else if (pProperty->Type == EvtVarTypeUInt32) {
            std::cout << "    Value: " << pProperty->UInt32Val << std::endl;
        } else if (pProperty->Type == EvtVarTypeUInt64) {
            std::cout << "    Value: " << pProperty->UInt64Val << std::endl;
        } else if (pProperty->Type == EvtVarTypeByte) {
            std::cout << "    Value: " << (int)pProperty->ByteVal << std::endl;
        } else if (pProperty->Type == EvtVarTypeNull) {
            std::cout << "    Value: (null)" << std::endl;
        } else {
            std::cout << "    Value: (unsupported type for display)" << std::endl;
        }
    }

    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "Testing getEventProperty() function:" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    std::string provider = getEventProperty(hEvent, EvtSystemProviderName);

    std::cout << "getEventProperty(EvtSystemEventID): '" << eventId << "'" << std::endl;
    std::cout << "getEventProperty(EvtSystemLevel): '" << level << "'" << std::endl;
    std::cout << "getEventProperty(EvtSystemChannel): '" << channel << "'" << std::endl;
    std::cout << "getEventProperty(EvtSystemComputer): '" << computer << "'" << std::endl;
    std::cout << "getEventProperty(EvtSystemProviderName): '" << provider << "'" << std::endl;

    // Cleanup
    delete[](BYTE*)pRenderedValues;
    EvtClose(hEvent);
    EvtClose(hResults);

    std::cout << std::endl << "========================================" << std::endl;
}

int main() {
    debugEventProperties();
    return 0;
}
