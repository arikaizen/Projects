/**
 * @file test_raw_event_formats.cpp
 * @brief Test program to demonstrate different event output formats
 *
 * This program retrieves a real Windows event and displays it in multiple formats:
 * - Plain Text (human-readable)
 * - JSON (for SIEM forwarding)
 * - Raw XML (complete Windows event data)
 * - Message only (event description)
 */

#include <iostream>
#include <windows.h>
#include <winevt.h>
#include "event_log_reader.h"

/**
 * @brief Get a test event from the Windows System log
 */
EVT_HANDLE getTestEvent() {
    EVT_HANDLE hResults = EvtQuery(
        NULL,
        L"System",
        L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection
    );

    if (hResults == NULL) {
        std::cout << "ERROR: Failed to open System log. Error: " << GetLastError() << std::endl;
        return NULL;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[1];

    if (EvtNext(hResults, 1, hEvents, 5000, 0, &dwReturned)) {
        EvtClose(hResults);
        if (dwReturned > 0) {
            return hEvents[0];
        }
    }

    EvtClose(hResults);
    std::cout << "ERROR: No events found in System log" << std::endl;
    return NULL;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log - Format Demonstration" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    // Get a real event from the System log
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        return 1;
    }

    std::cout << "Retrieved event from Windows System log" << std::endl << std::endl;

    // Format 1: Plain Text (human-readable)
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "FORMAT 1: PLAIN TEXT" << std::endl;
    std::cout << "========================================" << std::endl;
    std::string plainText = formatEventAsPlainText(hEvent);
    std::cout << plainText << std::endl << std::endl;

    // Format 2: JSON (for SIEM)
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "FORMAT 2: JSON (for SIEM forwarding)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::string jsonText = formatEventAsJson(hEvent);
    std::cout << jsonText << std::endl << std::endl;

    // Format 3: Event Message Only
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "FORMAT 3: MESSAGE ONLY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::string message = getEventMessage(hEvent);
    if (!message.empty()) {
        std::cout << message << std::endl;
    } else {
        std::cout << "(No message available)" << std::endl;
    }
    std::cout << std::endl;

    // Format 4: Raw XML (complete event data)
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "FORMAT 4: RAW XML" << std::endl;
    std::cout << "========================================" << std::endl;
    std::string rawXml = getRawEventXml(hEvent);
    if (!rawXml.empty()) {
        std::cout << rawXml << std::endl;
    } else {
        std::cout << "(Failed to retrieve XML)" << std::endl;
    }
    std::cout << std::endl;

    // Cleanup
    EvtClose(hEvent);

    std::cout << "========================================" << std::endl;
    std::cout << "All formats displayed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
