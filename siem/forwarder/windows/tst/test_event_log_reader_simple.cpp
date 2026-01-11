/**
 * @file test_event_log_reader_simple.cpp
 * @brief Simple standalone tests for Event Log Reader (no Google Test required)
 */

#include <iostream>
#include <windows.h>
#include <winevt.h>
#include "event_log_reader.h"
#include "logger.h"

int passed = 0;
int failed = 0;

#define TEST_START(name) std::cout << "Testing: " << name << "... "
#define TEST_PASS() { std::cout << "[PASS]" << std::endl; passed++; }
#define TEST_FAIL(msg) { std::cout << "[FAIL] " << msg << std::endl; failed++; }

#define ASSERT_TRUE(condition, msg) \
    if (!(condition)) { TEST_FAIL(msg); return false; }

#define ASSERT_FALSE(condition, msg) \
    if (condition) { TEST_FAIL(msg); return false; }

// Get a real event from System log
EVT_HANDLE getTestEvent() {
    EVT_HANDLE hResults = EvtQuery(NULL, L"System", L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection);

    if (hResults == NULL) return NULL;

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[1];

    if (EvtNext(hResults, 1, hEvents, 5000, 0, &dwReturned)) {
        EvtClose(hResults);
        if (dwReturned > 0) return hEvents[0];
    }

    EvtClose(hResults);
    return NULL;
}

bool test_GetEventProperty_EventID() {
    TEST_START("getEventProperty - Event ID");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    EvtClose(hEvent);

    ASSERT_FALSE(eventId.empty(), "Event ID is empty");
    TEST_PASS();
    return true;
}

bool test_GetEventProperty_Level() {
    TEST_START("getEventProperty - Level");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    EvtClose(hEvent);

    ASSERT_FALSE(level.empty(), "Level is empty");
    TEST_PASS();
    return true;
}

bool test_GetEventProperty_Channel() {
    TEST_START("getEventProperty - Channel");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    EvtClose(hEvent);

    ASSERT_FALSE(channel.empty(), "Channel is empty");
    TEST_PASS();
    return true;
}

bool test_GetEventProperty_Computer() {
    TEST_START("getEventProperty - Computer");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    EvtClose(hEvent);

    ASSERT_FALSE(computer.empty(), "Computer is empty");
    TEST_PASS();
    return true;
}

bool test_GetEventProperty_InvalidHandle() {
    TEST_START("getEventProperty - Invalid handle");
    std::string result = getEventProperty(NULL, EvtSystemEventID);
    ASSERT_TRUE(result.empty(), "Should return empty for NULL handle");
    TEST_PASS();
    return true;
}

bool test_FormatEventAsJson() {
    TEST_START("formatEventAsJson - Returns valid JSON");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string json = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    ASSERT_FALSE(json.empty(), "JSON is empty");
    ASSERT_TRUE(json.find("{") != std::string::npos, "Missing opening brace");
    ASSERT_TRUE(json.find("}") != std::string::npos, "Missing closing brace");
    TEST_PASS();
    return true;
}

bool test_GetRawEventXml() {
    TEST_START("getRawEventXml - Returns XML");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string xml = getRawEventXml(hEvent);
    EvtClose(hEvent);

    ASSERT_FALSE(xml.empty(), "XML is empty");
    ASSERT_TRUE(xml.find("<") != std::string::npos, "Missing XML tags");
    ASSERT_TRUE(xml.find("Event") != std::string::npos, "Missing Event element");
    TEST_PASS();
    return true;
}

bool test_GetRawEventXml_InvalidHandle() {
    TEST_START("getRawEventXml - Invalid handle");
    std::string xml = getRawEventXml(NULL);
    ASSERT_TRUE(xml.empty(), "Should return empty for NULL handle");
    TEST_PASS();
    return true;
}

bool test_GetEventMessage() {
    TEST_START("getEventMessage - Returns message");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string message = getEventMessage(hEvent);
    EvtClose(hEvent);

    // Message may be empty if metadata unavailable, so just check it doesn't crash
    TEST_PASS();
    return true;
}

bool test_FormatEventAsPlainText() {
    TEST_START("formatEventAsPlainText - Returns formatted text");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string text = formatEventAsPlainText(hEvent);
    EvtClose(hEvent);

    ASSERT_FALSE(text.empty(), "Plain text is empty");
    ASSERT_TRUE(text.find("Event ID") != std::string::npos, "Missing 'Event ID' field");
    ASSERT_TRUE(text.find("Level") != std::string::npos, "Missing 'Level' field");
    ASSERT_TRUE(text.find("===") != std::string::npos, "Missing separator");
    TEST_PASS();
    return true;
}

bool test_AllFormats_SameEvent() {
    TEST_START("All formats work on same event");
    EVT_HANDLE hEvent = getTestEvent();
    ASSERT_TRUE(hEvent != NULL, "No events available");

    std::string json = formatEventAsJson(hEvent);
    std::string xml = getRawEventXml(hEvent);
    std::string text = formatEventAsPlainText(hEvent);

    EvtClose(hEvent);

    ASSERT_FALSE(json.empty(), "JSON is empty");
    ASSERT_FALSE(xml.empty(), "XML is empty");
    ASSERT_FALSE(text.empty(), "Plain text is empty");
    TEST_PASS();
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Event Log Reader Tests (Standalone)" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    initializeGlobalLogger("test_simple.csv");

    // Run all tests
    test_GetEventProperty_EventID();
    test_GetEventProperty_Level();
    test_GetEventProperty_Channel();
    test_GetEventProperty_Computer();
    test_GetEventProperty_InvalidHandle();
    test_FormatEventAsJson();
    test_GetRawEventXml();
    test_GetRawEventXml_InvalidHandle();
    test_GetEventMessage();
    test_FormatEventAsPlainText();
    test_AllFormats_SameEvent();

    shutdownGlobalLogger();
    std::remove("test_simple.csv");

    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed == 0) ? 0 : 1;
}
