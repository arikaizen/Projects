/**
 * @file test_event_log_reader_standalone.cpp
 * @brief Standalone test program for Event Log Reader functions
 *
 * Tests the event log reader without requiring Google Test framework.
 * This is a simple executable that tests each function and prints results.
 */

#include "../inc/event_log_reader.h"
#include "../inc/logger.h"
#include <iostream>
#include <iomanip>
#include <winevt.h>

// ANSI color codes for output
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

// Test result tracking
int g_testsPassed = 0;
int g_testsFailed = 0;

// Helper function to print test results
void printTestResult(const std::string& testName, bool passed, const std::string& message = "") {
    if (passed) {
        std::cout << COLOR_GREEN << "[PASS] " << COLOR_RESET << testName << std::endl;
        if (!message.empty()) {
            std::cout << "       " << message << std::endl;
        }
        g_testsPassed++;
    } else {
        std::cout << COLOR_RED << "[FAIL] " << COLOR_RESET << testName << std::endl;
        if (!message.empty()) {
            std::cout << "       Error: " << message << std::endl;
        }
        g_testsFailed++;
    }
}

// Helper function to get a test event from System log
EVT_HANDLE getTestEvent() {
    EVT_HANDLE hResults = EvtQuery(
        NULL,
        L"System",
        L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection
    );

    if (hResults == NULL) {
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
    return NULL;
}

// Test 1: getEventProperty - Event ID
void test_getEventProperty_EventID() {
    std::cout << std::endl << COLOR_BLUE << "Test 1: getEventProperty - Event ID" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Get Event ID", false, "No events available in System log");
        return;
    }

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    bool passed = !eventId.empty();

    std::string msg = "Event ID: " + eventId;
    printTestResult("Get Event ID", passed, msg);

    EvtClose(hEvent);
}

// Test 2: getEventProperty - Level
void test_getEventProperty_Level() {
    std::cout << std::endl << COLOR_BLUE << "Test 2: getEventProperty - Level" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Get Level", false, "No events available");
        return;
    }

    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    bool passed = !level.empty();

    std::string levelName;
    if (level == "1") levelName = "Critical";
    else if (level == "2") levelName = "Error";
    else if (level == "3") levelName = "Warning";
    else if (level == "4") levelName = "Information";
    else if (level == "5") levelName = "Verbose";
    else levelName = "Unknown";

    std::string msg = "Level: " + level + " (" + levelName + ")";
    printTestResult("Get Level", passed, msg);

    EvtClose(hEvent);
}

// Test 3: getEventProperty - Channel
void test_getEventProperty_Channel() {
    std::cout << std::endl << COLOR_BLUE << "Test 3: getEventProperty - Channel" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Get Channel", false, "No events available");
        return;
    }

    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    bool passed = !channel.empty();

    std::string msg = "Channel: " + channel;
    printTestResult("Get Channel", passed, msg);

    EvtClose(hEvent);
}

// Test 4: getEventProperty - Computer
void test_getEventProperty_Computer() {
    std::cout << std::endl << COLOR_BLUE << "Test 4: getEventProperty - Computer" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Get Computer", false, "No events available");
        return;
    }

    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    bool passed = !computer.empty();

    std::string msg = "Computer: " + computer;
    printTestResult("Get Computer", passed, msg);

    EvtClose(hEvent);
}

// Test 5: getEventProperty - Provider Name
void test_getEventProperty_ProviderName() {
    std::cout << std::endl << COLOR_BLUE << "Test 5: getEventProperty - Provider Name" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Get Provider Name", false, "No events available");
        return;
    }

    std::string provider = getEventProperty(hEvent, EvtSystemProviderName);
    bool passed = !provider.empty();

    std::string msg = "Provider: " + provider;
    printTestResult("Get Provider Name", passed, msg);

    EvtClose(hEvent);
}

// Test 6: getEventProperty - Invalid Handle
void test_getEventProperty_InvalidHandle() {
    std::cout << std::endl << COLOR_BLUE << "Test 6: getEventProperty - Invalid Handle" << COLOR_RESET << std::endl;

    std::string result = getEventProperty(NULL, EvtSystemEventID);
    bool passed = result.empty();

    printTestResult("Invalid Handle Returns Empty", passed);
}

// Test 7: formatEventAsJson - Valid Event
void test_formatEventAsJson_ValidEvent() {
    std::cout << std::endl << COLOR_BLUE << "Test 7: formatEventAsJson - Valid Event" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("Format Event as JSON", false, "No events available");
        return;
    }

    std::string json = formatEventAsJson(hEvent);
    bool passed = !json.empty() &&
                  json.find("{") != std::string::npos &&
                  json.find("}") != std::string::npos;

    if (passed) {
        std::cout << COLOR_GREEN << "[PASS] " << COLOR_RESET << "Format Event as JSON" << std::endl;
        std::cout << "       JSON: " << json << std::endl;
        g_testsPassed++;
    } else {
        printTestResult("Format Event as JSON", false, "Invalid JSON structure");
    }

    EvtClose(hEvent);
}

// Test 8: formatEventAsJson - Contains Required Fields
void test_formatEventAsJson_RequiredFields() {
    std::cout << std::endl << COLOR_BLUE << "Test 8: formatEventAsJson - Required Fields" << COLOR_RESET << std::endl;

    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        printTestResult("JSON Contains Required Fields", false, "No events available");
        return;
    }

    std::string json = formatEventAsJson(hEvent);

    bool hasEventId = json.find("event_id") != std::string::npos;
    bool hasLevel = json.find("level") != std::string::npos;
    bool hasChannel = json.find("channel") != std::string::npos;
    bool hasComputer = json.find("computer") != std::string::npos;
    bool hasTimestamp = json.find("timestamp") != std::string::npos;

    bool passed = hasEventId && hasLevel && hasChannel && hasComputer && hasTimestamp;

    std::string msg;
    if (passed) {
        msg = "All required fields present";
    } else {
        msg = "Missing fields - ";
        if (!hasEventId) msg += "event_id ";
        if (!hasLevel) msg += "level ";
        if (!hasChannel) msg += "channel ";
        if (!hasComputer) msg += "computer ";
        if (!hasTimestamp) msg += "timestamp ";
    }

    printTestResult("JSON Contains Required Fields", passed, msg);

    EvtClose(hEvent);
}

// Test 9: formatEventAsJson - Invalid Handle
void test_formatEventAsJson_InvalidHandle() {
    std::cout << std::endl << COLOR_BLUE << "Test 9: formatEventAsJson - Invalid Handle" << COLOR_RESET << std::endl;

    std::string json = formatEventAsJson(NULL);
    bool passed = !json.empty() && json.find("{") != std::string::npos;

    std::string msg = "Returns: " + json;
    printTestResult("Invalid Handle Returns Error JSON", passed, msg);
}

// Test 10: getTimeString - Current Time
void test_getTimeString_CurrentTime() {
    std::cout << std::endl << COLOR_BLUE << "Test 10: getTimeString - Current Time" << COLOR_RESET << std::endl;

    std::wstring timeStr = getTimeString(0);
    bool passed = !timeStr.empty() &&
                  timeStr.find(L"T") != std::wstring::npos &&
                  timeStr.find(L"Z") != std::wstring::npos;

    // Convert to narrow string for display
    std::string narrowTimeStr(timeStr.begin(), timeStr.end());
    std::string msg = "Current Time: " + narrowTimeStr;
    printTestResult("Get Current Time String", passed, msg);
}

// Test 11: getTimeString - Past Time (24 hours ago)
void test_getTimeString_PastTime() {
    std::cout << std::endl << COLOR_BLUE << "Test 11: getTimeString - Past Time" << COLOR_RESET << std::endl;

    std::wstring timeStr = getTimeString(-24);
    bool passed = !timeStr.empty() &&
                  timeStr.find(L"T") != std::wstring::npos &&
                  timeStr.find(L"Z") != std::wstring::npos;

    std::string narrowTimeStr(timeStr.begin(), timeStr.end());
    std::string msg = "24 Hours Ago: " + narrowTimeStr;
    printTestResult("Get Past Time String", passed, msg);
}

// Test 12: buildHistoricalQuery - REALTIME Mode
void test_buildHistoricalQuery_Realtime() {
    std::cout << std::endl << COLOR_BLUE << "Test 12: buildHistoricalQuery - REALTIME Mode" << COLOR_RESET << std::endl;

    EventQueryConfig config;
    config.mode = EventReadMode::REALTIME;

    std::wstring query = buildHistoricalQuery(config);
    bool passed = query == L"*";

    std::string narrowQuery(query.begin(), query.end());
    std::string msg = "Query: " + narrowQuery;
    printTestResult("REALTIME Mode Query", passed, msg);
}

// Test 13: buildHistoricalQuery - HISTORICAL_ALL Mode
void test_buildHistoricalQuery_HistoricalAll() {
    std::cout << std::endl << COLOR_BLUE << "Test 13: buildHistoricalQuery - HISTORICAL_ALL Mode" << COLOR_RESET << std::endl;

    EventQueryConfig config;
    config.mode = EventReadMode::HISTORICAL_ALL;

    std::wstring query = buildHistoricalQuery(config);
    bool passed = query == L"*";

    std::string narrowQuery(query.begin(), query.end());
    std::string msg = "Query: " + narrowQuery;
    printTestResult("HISTORICAL_ALL Mode Query", passed, msg);
}

// Test 14: buildHistoricalQuery - HISTORICAL_RECENT Mode
void test_buildHistoricalQuery_HistoricalRecent() {
    std::cout << std::endl << COLOR_BLUE << "Test 14: buildHistoricalQuery - HISTORICAL_RECENT Mode" << COLOR_RESET << std::endl;

    EventQueryConfig config;
    config.mode = EventReadMode::HISTORICAL_RECENT;
    config.hoursBack = 24;

    std::wstring query = buildHistoricalQuery(config);
    bool passed = query.find(L"TimeCreated") != std::wstring::npos &&
                  query.find(L"@SystemTime>=") != std::wstring::npos;

    std::string narrowQuery(query.begin(), query.end());
    std::string msg = "Query contains time filter";
    printTestResult("HISTORICAL_RECENT Mode Query", passed, msg);
}

// Test 15: Multiple Events Processing
void test_MultipleEvents() {
    std::cout << std::endl << COLOR_BLUE << "Test 15: Multiple Events Processing" << COLOR_RESET << std::endl;

    EVT_HANDLE hResults = EvtQuery(
        NULL,
        L"System",
        L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection
    );

    if (hResults == NULL) {
        printTestResult("Process Multiple Events", false, "Cannot query System log");
        return;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[5];

    if (EvtNext(hResults, 5, hEvents, 5000, 0, &dwReturned)) {
        bool allSuccess = true;

        for (DWORD i = 0; i < dwReturned; i++) {
            std::string json = formatEventAsJson(hEvents[i]);
            if (json.empty() || json.find("{") == std::string::npos) {
                allSuccess = false;
            }
            EvtClose(hEvents[i]);
        }

        std::string msg = "Processed " + std::to_string(dwReturned) + " events";
        printTestResult("Process Multiple Events", allSuccess, msg);
    } else {
        printTestResult("Process Multiple Events", false, "Failed to retrieve events");
    }

    EvtClose(hResults);
}

int main() {
    // Enable ANSI color codes on Windows 10+
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(hConsole, &consoleMode);
    SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::cout << "========================================" << std::endl;
    std::cout << "  Event Log Reader - Standalone Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // Initialize logger
    initializeGlobalLogger("test_standalone_event_reader.csv");

    // Run all tests
    test_getEventProperty_EventID();
    test_getEventProperty_Level();
    test_getEventProperty_Channel();
    test_getEventProperty_Computer();
    test_getEventProperty_ProviderName();
    test_getEventProperty_InvalidHandle();
    test_formatEventAsJson_ValidEvent();
    test_formatEventAsJson_RequiredFields();
    test_formatEventAsJson_InvalidHandle();
    test_getTimeString_CurrentTime();
    test_getTimeString_PastTime();
    test_buildHistoricalQuery_Realtime();
    test_buildHistoricalQuery_HistoricalAll();
    test_buildHistoricalQuery_HistoricalRecent();
    test_MultipleEvents();

    // Print summary
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << COLOR_GREEN << "  Passed: " << g_testsPassed << COLOR_RESET << std::endl;
    std::cout << COLOR_RED << "  Failed: " << g_testsFailed << COLOR_RESET << std::endl;
    std::cout << "  Total:  " << (g_testsPassed + g_testsFailed) << std::endl;
    std::cout << "========================================" << std::endl;

    if (g_testsFailed == 0) {
        std::cout << std::endl << COLOR_GREEN << "✓✓✓ ALL TESTS PASSED ✓✓✓" << COLOR_RESET << std::endl;
    } else {
        std::cout << std::endl << COLOR_RED << "✗✗✗ SOME TESTS FAILED ✗✗✗" << COLOR_RESET << std::endl;
    }

    // Cleanup
    shutdownGlobalLogger();
    std::remove("test_standalone_event_reader.csv");

    return (g_testsFailed == 0) ? 0 : 1;
}
