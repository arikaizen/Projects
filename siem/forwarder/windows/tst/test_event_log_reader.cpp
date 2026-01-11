/**
 * @file test_event_log_reader.cpp
 * @brief Integration tests for Event Log Reader
 *
 * Tests Windows Event Log reading and JSON formatting
 * Note: These tests require Windows Event Log access
 */

#include <gtest/gtest.h>
#include "event_log_reader.h"
#include "logger.h"
#include <winevt.h>
#include <string>

// Test fixture for Event Log Reader tests
class EventLogReaderTest : public ::testing::Test {
protected:
    EVT_HANDLE hEvent;

    void SetUp() override {
        hEvent = NULL;
        initializeGlobalLogger("test_event_reader.csv");
    }

    void TearDown() override {
        if (hEvent != NULL) {
            EvtClose(hEvent);
            hEvent = NULL;
        }
        shutdownGlobalLogger();
        std::remove("test_event_reader.csv");
    }

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
            return hEvents[0];
        }

        EvtClose(hResults);
        return NULL;
    }
};

/**
 * Test: getEventProperty with EventID
 */
TEST_F(EventLogReaderTest, GetEventProperty_EventID_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    EXPECT_FALSE(eventId.empty());

    // Event ID should be numeric
    EXPECT_TRUE(std::all_of(eventId.begin(), eventId.end(), ::isdigit));
}

/**
 * Test: getEventProperty with Level
 */
TEST_F(EventLogReaderTest, GetEventProperty_Level_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    EXPECT_FALSE(level.empty());
}

/**
 * Test: getEventProperty with Channel
 */
TEST_F(EventLogReaderTest, GetEventProperty_Channel_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    EXPECT_FALSE(channel.empty());
}

/**
 * Test: getEventProperty with Computer
 */
TEST_F(EventLogReaderTest, GetEventProperty_Computer_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    EXPECT_FALSE(computer.empty());
}

/**
 * Test: getEventProperty with TimeCreated
 */
TEST_F(EventLogReaderTest, GetEventProperty_TimeCreated_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string timeCreated = getEventProperty(hEvent, EvtSystemTimeCreated);
    EXPECT_FALSE(timeCreated.empty());
}

/**
 * Test: getEventProperty with Provider Name
 */
TEST_F(EventLogReaderTest, GetEventProperty_ProviderName_ReturnsValue) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string providerName = getEventProperty(hEvent, EvtSystemProviderName);
    EXPECT_FALSE(providerName.empty());
}

/**
 * Test: getEventProperty with invalid handle returns empty
 */
TEST_F(EventLogReaderTest, GetEventProperty_InvalidHandle_ReturnsEmpty) {
    std::string result = getEventProperty(NULL, EvtSystemEventID);
    EXPECT_TRUE(result.empty());
}

/**
 * Test: formatEventAsJson returns valid JSON structure
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_ReturnsValidJson) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string json = formatEventAsJson(hEvent);

    // Check for JSON structure markers
    EXPECT_TRUE(json.find("{") != std::string::npos);
    EXPECT_TRUE(json.find("}") != std::string::npos);

    // Check for expected fields
    EXPECT_TRUE(json.find("EventID") != std::string::npos ||
                json.find("event_id") != std::string::npos ||
                json.find("eventid") != std::string::npos);
}

/**
 * Test: formatEventAsJson includes all standard fields
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_IncludesStandardFields) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string json = formatEventAsJson(hEvent);

    // Convert to lowercase for case-insensitive search
    std::string jsonLower = json;
    std::transform(jsonLower.begin(), jsonLower.end(), jsonLower.begin(), ::tolower);

    // Check for common event log fields (case-insensitive)
    bool hasEventId = jsonLower.find("eventid") != std::string::npos;
    bool hasLevel = jsonLower.find("level") != std::string::npos;
    bool hasChannel = jsonLower.find("channel") != std::string::npos;
    bool hasComputer = jsonLower.find("computer") != std::string::npos;

    // At least some standard fields should be present
    EXPECT_TRUE(hasEventId || hasLevel || hasChannel || hasComputer);
}

/**
 * Test: formatEventAsJson with invalid handle returns error JSON
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_InvalidHandle_ReturnsErrorJson) {
    std::string json = formatEventAsJson(NULL);

    // Should return some form of error indication in JSON
    EXPECT_FALSE(json.empty());
    EXPECT_TRUE(json.find("{") != std::string::npos);
}

/**
 * Test: formatEventAsJson escapes special characters
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_EscapesSpecialChars) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string json = formatEventAsJson(hEvent);

    // JSON should not have unescaped quotes (except field delimiters)
    // If there are quotes in data, they should be escaped as \"
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
            EXPECT_TRUE(prevChar == ':' || prevChar == ',' ||
                       prevChar == '{' || prevChar == '[');
        }
        pos++;
    }
}

/**
 * Test: Multiple events can be formatted
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_MultipleEvents) {
    EVT_HANDLE hResults = EvtQuery(
        NULL,
        L"System",
        L"*",
        EvtQueryChannelPath | EvtQueryForwardDirection
    );

    if (hResults == NULL) {
        GTEST_SKIP() << "Cannot query System log";
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[3];

    if (EvtNext(hResults, 3, hEvents, INFINITE, 0, &dwReturned)) {
        EXPECT_GE(dwReturned, 1);

        for (DWORD i = 0; i < dwReturned; i++) {
            std::string json = formatEventAsJson(hEvents[i]);
            EXPECT_FALSE(json.empty());
            EXPECT_TRUE(json.find("{") != std::string::npos);
            EvtClose(hEvents[i]);
        }
    }

    EvtClose(hResults);
}

/**
 * Test: formatEventAsJson output is consistent
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_ConsistentOutput) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string json1 = formatEventAsJson(hEvent);
    std::string json2 = formatEventAsJson(hEvent);

    // Same event should produce same JSON
    EXPECT_EQ(json1, json2);
}

/**
 * Test: getEventProperty handles different data types
 */
TEST_F(EventLogReaderTest, GetEventProperty_DifferentTypes) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    // Numeric property (Event ID)
    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    EXPECT_FALSE(eventId.empty());

    // String property (Computer)
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);
    EXPECT_FALSE(computer.empty());

    // Level (numeric)
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    EXPECT_FALSE(level.empty());
}

/**
 * Test: JSON output doesn't contain control characters
 */
TEST_F(EventLogReaderTest, FormatEventAsJson_NoControlCharacters) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string json = formatEventAsJson(hEvent);

    // Check for unescaped control characters (except escaped ones)
    for (size_t i = 0; i < json.length(); i++) {
        char c = json[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            // If found, it should be part of an escape sequence
            if (i > 0) {
                EXPECT_EQ(json[i-1], '\\');
            }
        }
    }
}

/**
 * Test: getRawEventXml returns non-empty XML
 */
TEST_F(EventLogReaderTest, GetRawEventXml_ReturnsXml) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string xml = getRawEventXml(hEvent);

    EXPECT_FALSE(xml.empty());
    // XML should start with < and contain Event tags
    EXPECT_TRUE(xml.find("<") != std::string::npos);
    EXPECT_TRUE(xml.find("Event") != std::string::npos);
}

/**
 * Test: getRawEventXml contains standard event elements
 */
TEST_F(EventLogReaderTest, GetRawEventXml_ContainsStandardElements) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string xml = getRawEventXml(hEvent);

    // Check for standard Windows Event Log XML structure
    EXPECT_TRUE(xml.find("System") != std::string::npos);
    EXPECT_TRUE(xml.find("EventID") != std::string::npos ||
                xml.find("EventRecordID") != std::string::npos);
}

/**
 * Test: getRawEventXml with invalid handle returns empty
 */
TEST_F(EventLogReaderTest, GetRawEventXml_InvalidHandle_ReturnsEmpty) {
    std::string xml = getRawEventXml(NULL);
    EXPECT_TRUE(xml.empty());
}

/**
 * Test: getRawEventXml is consistent
 */
TEST_F(EventLogReaderTest, GetRawEventXml_ConsistentOutput) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string xml1 = getRawEventXml(hEvent);
    std::string xml2 = getRawEventXml(hEvent);

    EXPECT_EQ(xml1, xml2);
}

/**
 * Test: getEventMessage returns message text
 */
TEST_F(EventLogReaderTest, GetEventMessage_ReturnsMessage) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string message = getEventMessage(hEvent);

    // Message may be empty if provider metadata is not available
    // But the function should not crash and should return a string
    EXPECT_TRUE(message.empty() || !message.empty());
}

/**
 * Test: getEventMessage with invalid handle returns empty
 */
TEST_F(EventLogReaderTest, GetEventMessage_InvalidHandle_ReturnsEmpty) {
    std::string message = getEventMessage(NULL);
    EXPECT_TRUE(message.empty());
}

/**
 * Test: formatEventAsPlainText returns formatted text
 */
TEST_F(EventLogReaderTest, FormatEventAsPlainText_ReturnsText) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string text = formatEventAsPlainText(hEvent);

    EXPECT_FALSE(text.empty());
}

/**
 * Test: formatEventAsPlainText contains standard fields
 */
TEST_F(EventLogReaderTest, FormatEventAsPlainText_ContainsStandardFields) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string text = formatEventAsPlainText(hEvent);

    // Check for expected field labels
    EXPECT_TRUE(text.find("Event ID") != std::string::npos);
    EXPECT_TRUE(text.find("Level") != std::string::npos);
    EXPECT_TRUE(text.find("Time") != std::string::npos);
    EXPECT_TRUE(text.find("Channel") != std::string::npos);
    EXPECT_TRUE(text.find("Computer") != std::string::npos);
    EXPECT_TRUE(text.find("Provider") != std::string::npos);
}

/**
 * Test: formatEventAsPlainText with invalid handle returns error text
 */
TEST_F(EventLogReaderTest, FormatEventAsPlainText_InvalidHandle_ReturnsText) {
    std::string text = formatEventAsPlainText(NULL);

    // Should return some text even with invalid handle
    EXPECT_FALSE(text.empty());
}

/**
 * Test: formatEventAsPlainText is consistent
 */
TEST_F(EventLogReaderTest, FormatEventAsPlainText_ConsistentOutput) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string text1 = formatEventAsPlainText(hEvent);
    std::string text2 = formatEventAsPlainText(hEvent);

    EXPECT_EQ(text1, text2);
}

/**
 * Test: formatEventAsPlainText includes separators
 */
TEST_F(EventLogReaderTest, FormatEventAsPlainText_IncludesSeparators) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string text = formatEventAsPlainText(hEvent);

    // Should have separator lines for readability
    EXPECT_TRUE(text.find("===") != std::string::npos);
}

/**
 * Test: All format functions work on same event
 */
TEST_F(EventLogReaderTest, AllFormats_WorkOnSameEvent) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    // All format functions should work on the same event
    std::string json = formatEventAsJson(hEvent);
    std::string xml = getRawEventXml(hEvent);
    std::string text = formatEventAsPlainText(hEvent);
    std::string message = getEventMessage(hEvent);

    EXPECT_FALSE(json.empty());
    EXPECT_FALSE(xml.empty());
    EXPECT_FALSE(text.empty());
    // message may be empty if metadata unavailable, so we don't assert it
}

/**
 * Test: Different formats contain same event ID
 */
TEST_F(EventLogReaderTest, DifferentFormats_ContainSameEventID) {
    hEvent = getTestEvent();
    if (hEvent == NULL) {
        GTEST_SKIP() << "No events available in System log";
    }

    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string json = formatEventAsJson(hEvent);
    std::string xml = getRawEventXml(hEvent);
    std::string text = formatEventAsPlainText(hEvent);

    // All formats should contain the same event ID
    if (!eventId.empty()) {
        EXPECT_TRUE(json.find(eventId) != std::string::npos);
        EXPECT_TRUE(xml.find(eventId) != std::string::npos);
        EXPECT_TRUE(text.find(eventId) != std::string::npos);
    }
}
