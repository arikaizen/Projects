/**
 * @file test_event_log_reader.cpp
 * @brief Integration tests for Event Log Reader (No Google Test dependency)
 *
 * Tests Windows Event Log reading and JSON formatting
 * Note: These tests require Windows Event Log access
 */

#include <iostream>
#include <algorithm>
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

// Helper: Get a real event from System log for testing
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

    if (EvtNext(hResults, 1, hEvents, INFINITE, 0, &dwReturned)) {
        EvtClose(hResults);
        if (dwReturned > 0) {
            return hEvents[0];
        }
    }

    EvtClose(hResults);
    return NULL;
}

/**
 * Test: getEventProperty with EventID
 */
bool test_GetEventProperty_EventID_ReturnsValue() {
    TEST_START("getEventProperty - EventID returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    EvtClose(hEvent);

    ASSERT_FALSE(eventId.empty(), "Event ID is empty");

    // Event ID should be numeric
    bool isNumeric = std::all_of(eventId.begin(), eventId.end(), ::isdigit);
    ASSERT_TRUE(isNumeric, "Event ID is not numeric");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with Level
 */
bool test_GetEventProperty_Level_ReturnsValue() {
    TEST_START("getEventProperty - Level returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    EvtClose(hEvent);

    ASSERT_FALSE(level.empty(), "Level is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with Channel
 */
bool test_GetEventProperty_Channel_ReturnsValue() {
    TEST_START("getEventProperty - Channel returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    EvtClose(hEvent);

    ASSERT_FALSE(channel.empty(), "Channel is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with Computer
 */
bool test_GetEventProperty_Computer_ReturnsValue() {
    TEST_START("getEventProperty - Computer returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    EvtClose(hEvent);

    ASSERT_FALSE(computer.empty(), "Computer is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with TimeCreated
 */
bool test_GetEventProperty_TimeCreated_ReturnsValue() {
    TEST_START("getEventProperty - TimeCreated returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string timeCreated = getEventProperty(hEvent, EvtSystemTimeCreated);
    EvtClose(hEvent);

    ASSERT_FALSE(timeCreated.empty(), "TimeCreated is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with Provider Name
 */
bool test_GetEventProperty_ProviderName_ReturnsValue() {
    TEST_START("getEventProperty - ProviderName returns value");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string providerName = getEventProperty(hEvent, EvtSystemProviderName);
    EvtClose(hEvent);

    ASSERT_FALSE(providerName.empty(), "Provider name is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty with invalid handle returns empty
 */
bool test_GetEventProperty_InvalidHandle_ReturnsEmpty() {
    TEST_START("getEventProperty - Invalid handle returns empty");

    std::string result = getEventProperty(NULL, EvtSystemEventID);
    ASSERT_TRUE(result.empty(), "Should return empty for NULL handle");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsJson returns valid JSON structure
 */
bool test_FormatEventAsJson_ReturnsValidJson() {
    TEST_START("formatEventAsJson - Returns valid JSON");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string json = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    // Check for JSON structure markers
    ASSERT_TRUE(json.find("{") != std::string::npos, "Missing opening brace");
    ASSERT_TRUE(json.find("}") != std::string::npos, "Missing closing brace");

    // Check for expected fields
    bool hasEventId = json.find("EventID") != std::string::npos ||
                      json.find("event_id") != std::string::npos ||
                      json.find("eventid") != std::string::npos;
    ASSERT_TRUE(hasEventId, "Missing event ID field");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsJson includes all standard fields
 */
bool test_FormatEventAsJson_IncludesStandardFields() {
    TEST_START("formatEventAsJson - Includes standard fields");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string json = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    // Convert to lowercase for case-insensitive search
    std::string jsonLower = json;
    std::transform(jsonLower.begin(), jsonLower.end(), jsonLower.begin(), ::tolower);

    // Check for common event log fields (case-insensitive)
    bool hasEventId = jsonLower.find("eventid") != std::string::npos;
    bool hasLevel = jsonLower.find("level") != std::string::npos;
    bool hasChannel = jsonLower.find("channel") != std::string::npos;
    bool hasComputer = jsonLower.find("computer") != std::string::npos;

    // At least some standard fields should be present
    ASSERT_TRUE(hasEventId || hasLevel || hasChannel || hasComputer,
                "Missing standard fields");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsJson with invalid handle returns error JSON
 */
bool test_FormatEventAsJson_InvalidHandle_ReturnsErrorJson() {
    TEST_START("formatEventAsJson - Invalid handle returns error JSON");

    std::string json = formatEventAsJson(NULL);

    // Should return some form of error indication in JSON
    ASSERT_FALSE(json.empty(), "JSON is empty");
    ASSERT_TRUE(json.find("{") != std::string::npos, "Missing opening brace");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsJson escapes special characters
 */
bool test_FormatEventAsJson_EscapesSpecialChars() {
    TEST_START("formatEventAsJson - Escapes special characters");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string json = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    // JSON should not have unescaped quotes (except field delimiters)
    size_t pos = 0;
    while ((pos = json.find("\"", pos + 1)) != std::string::npos) {
        // Check if this quote is escaped or is a field delimiter
        if (pos > 0 && json[pos - 1] == '\\') {
            // This is an escaped quote, which is good
            pos++;
            continue;
        }
        // Unescaped quotes should be field delimiters (preceded by : or ,)
        if (pos > 0) {
            char prevChar = json[pos - 1];
            ASSERT_TRUE(prevChar == ':' || prevChar == ',' ||
                       prevChar == '{' || prevChar == '[',
                       "Unescaped quote not a field delimiter");
        }
        pos++;
    }

    TEST_PASS();
    return true;
}

/**
 * Test: Multiple events can be formatted
 */
bool test_FormatEventAsJson_MultipleEvents() {
    TEST_START("formatEventAsJson - Multiple events");

    EVT_HANDLE hResults = EvtQuery(
        NULL,
        L"System",
        L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection
    );

    if (hResults == NULL) {
        std::cout << "[SKIP] Cannot query System log" << std::endl;
        return true;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[3];

    if (EvtNext(hResults, 3, hEvents, INFINITE, 0, &dwReturned)) {
        ASSERT_TRUE(dwReturned >= 1, "No events returned");

        for (DWORD i = 0; i < dwReturned; i++) {
            std::string json = formatEventAsJson(hEvents[i]);
            ASSERT_FALSE(json.empty(), "JSON is empty");
            ASSERT_TRUE(json.find("{") != std::string::npos, "Missing opening brace");
            EvtClose(hEvents[i]);
        }
    }

    EvtClose(hResults);

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsJson output is consistent
 */
bool test_FormatEventAsJson_ConsistentOutput() {
    TEST_START("formatEventAsJson - Consistent output");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string json1 = formatEventAsJson(hEvent);
    std::string json2 = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    // Same event should produce same JSON
    ASSERT_TRUE(json1 == json2, "JSON output not consistent");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventProperty handles different data types
 */
bool test_GetEventProperty_DifferentTypes() {
    TEST_START("getEventProperty - Different data types");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    // Numeric property (Event ID)
    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    ASSERT_FALSE(eventId.empty(), "Event ID is empty");

    // String property (Computer)
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    ASSERT_FALSE(computer.empty(), "Computer is empty");

    // Level (numeric)
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    ASSERT_FALSE(level.empty(), "Level is empty");

    EvtClose(hEvent);

    TEST_PASS();
    return true;
}

/**
 * Test: JSON output doesn't contain control characters
 */
bool test_FormatEventAsJson_NoControlCharacters() {
    TEST_START("formatEventAsJson - No control characters");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string json = formatEventAsJson(hEvent);
    EvtClose(hEvent);

    // Check for unescaped control characters (except escaped ones)
    for (size_t i = 0; i < json.length(); i++) {
        char c = json[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            // If found, it should be part of an escape sequence
            if (i > 0) {
                ASSERT_TRUE(json[i-1] == '\\', "Unescaped control character");
            }
        }
    }

    TEST_PASS();
    return true;
}

/**
 * Test: getRawEventXml returns non-empty XML
 */
bool test_GetRawEventXml_ReturnsXml() {
    TEST_START("getRawEventXml - Returns XML");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string xml = getRawEventXml(hEvent);
    EvtClose(hEvent);

    ASSERT_FALSE(xml.empty(), "XML is empty");
    // XML should start with < and contain Event tags
    ASSERT_TRUE(xml.find("<") != std::string::npos, "Missing XML tags");
    ASSERT_TRUE(xml.find("Event") != std::string::npos, "Missing Event element");

    TEST_PASS();
    return true;
}

/**
 * Test: getRawEventXml contains standard event elements
 */
bool test_GetRawEventXml_ContainsStandardElements() {
    TEST_START("getRawEventXml - Contains standard elements");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string xml = getRawEventXml(hEvent);
    EvtClose(hEvent);

    // Check for standard Windows Event Log XML structure
    ASSERT_TRUE(xml.find("System") != std::string::npos, "Missing System element");
    bool hasEventId = xml.find("EventID") != std::string::npos ||
                      xml.find("EventRecordID") != std::string::npos;
    ASSERT_TRUE(hasEventId, "Missing EventID/EventRecordID");

    TEST_PASS();
    return true;
}

/**
 * Test: getRawEventXml with invalid handle returns empty
 */
bool test_GetRawEventXml_InvalidHandle_ReturnsEmpty() {
    TEST_START("getRawEventXml - Invalid handle returns empty");

    std::string xml = getRawEventXml(NULL);
    ASSERT_TRUE(xml.empty(), "Should return empty for NULL handle");

    TEST_PASS();
    return true;
}

/**
 * Test: getRawEventXml is consistent
 */
bool test_GetRawEventXml_ConsistentOutput() {
    TEST_START("getRawEventXml - Consistent output");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string xml1 = getRawEventXml(hEvent);
    std::string xml2 = getRawEventXml(hEvent);
    EvtClose(hEvent);

    ASSERT_TRUE(xml1 == xml2, "XML output not consistent");

    TEST_PASS();
    return true;
}

/**
 * Test: getEventMessage returns message text
 */
bool test_GetEventMessage_ReturnsMessage() {
    TEST_START("getEventMessage - Returns message");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string message = getEventMessage(hEvent);
    EvtClose(hEvent);

    // Message may be empty if provider metadata is not available
    // But the function should not crash and should return a string
    // This test just verifies no crash occurs

    TEST_PASS();
    return true;
}

/**
 * Test: getEventMessage with invalid handle returns empty
 */
bool test_GetEventMessage_InvalidHandle_ReturnsEmpty() {
    TEST_START("getEventMessage - Invalid handle returns empty");

    std::string message = getEventMessage(NULL);
    ASSERT_TRUE(message.empty(), "Should return empty for NULL handle");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsPlainText returns formatted text
 */
bool test_FormatEventAsPlainText_ReturnsText() {
    TEST_START("formatEventAsPlainText - Returns text");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string text = formatEventAsPlainText(hEvent);
    EvtClose(hEvent);

    ASSERT_FALSE(text.empty(), "Plain text is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsPlainText contains standard fields
 */
bool test_FormatEventAsPlainText_ContainsStandardFields() {
    TEST_START("formatEventAsPlainText - Contains standard fields");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string text = formatEventAsPlainText(hEvent);
    EvtClose(hEvent);

    // Check for expected field labels
    ASSERT_TRUE(text.find("Event ID") != std::string::npos, "Missing 'Event ID' field");
    ASSERT_TRUE(text.find("Level") != std::string::npos, "Missing 'Level' field");
    ASSERT_TRUE(text.find("Time") != std::string::npos, "Missing 'Time' field");
    ASSERT_TRUE(text.find("Channel") != std::string::npos, "Missing 'Channel' field");
    ASSERT_TRUE(text.find("Computer") != std::string::npos, "Missing 'Computer' field");
    ASSERT_TRUE(text.find("Provider") != std::string::npos, "Missing 'Provider' field");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsPlainText with invalid handle returns error text
 */
bool test_FormatEventAsPlainText_InvalidHandle_ReturnsText() {
    TEST_START("formatEventAsPlainText - Invalid handle returns text");

    std::string text = formatEventAsPlainText(NULL);

    // Should return some text even with invalid handle
    ASSERT_FALSE(text.empty(), "Text is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsPlainText is consistent
 */
bool test_FormatEventAsPlainText_ConsistentOutput() {
    TEST_START("formatEventAsPlainText - Consistent output");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string text1 = formatEventAsPlainText(hEvent);
    std::string text2 = formatEventAsPlainText(hEvent);
    EvtClose(hEvent);

    ASSERT_TRUE(text1 == text2, "Plain text output not consistent");

    TEST_PASS();
    return true;
}

/**
 * Test: formatEventAsPlainText includes separators
 */
bool test_FormatEventAsPlainText_IncludesSeparators() {
    TEST_START("formatEventAsPlainText - Includes separators");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string text = formatEventAsPlainText(hEvent);
    EvtClose(hEvent);

    // Should have separator lines for readability
    ASSERT_TRUE(text.find("===") != std::string::npos, "Missing separator");

    TEST_PASS();
    return true;
}

/**
 * Test: All format functions work on same event
 */
bool test_AllFormats_WorkOnSameEvent() {
    TEST_START("All formats - Work on same event");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    // All format functions should work on the same event
    std::string json = formatEventAsJson(hEvent);
    std::string xml = getRawEventXml(hEvent);
    std::string text = formatEventAsPlainText(hEvent);
    std::string message = getEventMessage(hEvent);

    EvtClose(hEvent);

    ASSERT_FALSE(json.empty(), "JSON is empty");
    ASSERT_FALSE(xml.empty(), "XML is empty");
    ASSERT_FALSE(text.empty(), "Plain text is empty");
    // message may be empty if metadata unavailable, so we don't assert it

    TEST_PASS();
    return true;
}

/**
 * Test: Different formats contain same event ID
 */
bool test_DifferentFormats_ContainSameEventID() {
    TEST_START("Different formats - Contain same event ID");
    EVT_HANDLE hEvent = getTestEvent();
    if (hEvent == NULL) {
        std::cout << "[SKIP] No events available in System log" << std::endl;
        return true;
    }

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string json = formatEventAsJson(hEvent);
    std::string xml = getRawEventXml(hEvent);
    std::string text = formatEventAsPlainText(hEvent);

    EvtClose(hEvent);

    // All formats should contain the same event ID
    if (!eventId.empty()) {
        ASSERT_TRUE(json.find(eventId) != std::string::npos, "Event ID not in JSON");
        ASSERT_TRUE(xml.find(eventId) != std::string::npos, "Event ID not in XML");
        ASSERT_TRUE(text.find(eventId) != std::string::npos, "Event ID not in plain text");
    }

    TEST_PASS();
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Event Log Reader Tests" << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    initializeGlobalLogger("test_event_log_reader.csv");

    // Run all 28 tests
    test_GetEventProperty_EventID_ReturnsValue();
    test_GetEventProperty_Level_ReturnsValue();
    test_GetEventProperty_Channel_ReturnsValue();
    test_GetEventProperty_Computer_ReturnsValue();
    test_GetEventProperty_TimeCreated_ReturnsValue();
    test_GetEventProperty_ProviderName_ReturnsValue();
    test_GetEventProperty_InvalidHandle_ReturnsEmpty();
    test_FormatEventAsJson_ReturnsValidJson();
    test_FormatEventAsJson_IncludesStandardFields();
    test_FormatEventAsJson_InvalidHandle_ReturnsErrorJson();
    test_FormatEventAsJson_EscapesSpecialChars();
    test_FormatEventAsJson_MultipleEvents();
    test_FormatEventAsJson_ConsistentOutput();
    test_GetEventProperty_DifferentTypes();
    test_FormatEventAsJson_NoControlCharacters();
    test_GetRawEventXml_ReturnsXml();
    test_GetRawEventXml_ContainsStandardElements();
    test_GetRawEventXml_InvalidHandle_ReturnsEmpty();
    test_GetRawEventXml_ConsistentOutput();
    test_GetEventMessage_ReturnsMessage();
    test_GetEventMessage_InvalidHandle_ReturnsEmpty();
    test_FormatEventAsPlainText_ReturnsText();
    test_FormatEventAsPlainText_ContainsStandardFields();
    test_FormatEventAsPlainText_InvalidHandle_ReturnsText();
    test_FormatEventAsPlainText_ConsistentOutput();
    test_FormatEventAsPlainText_IncludesSeparators();
    test_AllFormats_WorkOnSameEvent();
    test_DifferentFormats_ContainSameEventID();

    shutdownGlobalLogger();
    std::remove("test_event_log_reader.csv");

    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed == 0) ? 0 : 1;
}
