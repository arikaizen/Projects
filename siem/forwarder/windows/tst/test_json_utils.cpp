/**
 * @file test_json_utils.cpp
 * @brief Unit tests for JSON utilities
 *
 * Tests JSON string escaping functionality
 */

#include <gtest/gtest.h>
#include "../inc/json_utils.h"

// Test fixture for JSON utils tests
class JsonUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

/**
 * Test: escapeJson with simple string (no special characters)
 */
TEST_F(JsonUtilsTest, EscapeJson_SimpleString) {
    std::string input = "Hello World";
    std::string expected = "Hello World";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with empty string
 */
TEST_F(JsonUtilsTest, EscapeJson_EmptyString) {
    std::string input = "";
    std::string expected = "";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with quotes
 */
TEST_F(JsonUtilsTest, EscapeJson_WithQuotes) {
    std::string input = "He said \"Hello\"";
    std::string expected = "He said \\\"Hello\\\"";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with backslashes
 */
TEST_F(JsonUtilsTest, EscapeJson_WithBackslashes) {
    std::string input = "C:\\Windows\\System32";
    std::string expected = "C:\\\\Windows\\\\System32";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with newlines
 */
TEST_F(JsonUtilsTest, EscapeJson_WithNewlines) {
    std::string input = "Line 1\nLine 2";
    std::string expected = "Line 1\\nLine 2";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with tabs
 */
TEST_F(JsonUtilsTest, EscapeJson_WithTabs) {
    std::string input = "Column1\tColumn2";
    std::string expected = "Column1\\tColumn2";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with carriage return
 */
TEST_F(JsonUtilsTest, EscapeJson_WithCarriageReturn) {
    std::string input = "Line 1\rLine 2";
    std::string expected = "Line 1\\rLine 2";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with forward slash
 */
TEST_F(JsonUtilsTest, EscapeJson_WithForwardSlash) {
    std::string input = "path/to/file";
    std::string expected = "path\\/to\\/file";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with mixed special characters
 */
TEST_F(JsonUtilsTest, EscapeJson_MixedSpecialCharacters) {
    std::string input = "Path: \"C:\\Windows\"\nStatus: OK";
    std::string expected = "Path: \\\"C:\\\\Windows\\\"\\nStatus: OK";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with unicode characters (should pass through)
 */
TEST_F(JsonUtilsTest, EscapeJson_UnicodeCharacters) {
    std::string input = "Hello 世界";
    std::string expected = "Hello 世界";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with control characters
 */
TEST_F(JsonUtilsTest, EscapeJson_ControlCharacters) {
    std::string input = "Text\x08with\x0Ccontrol";
    std::string expected = "Text\\bwith\\fcontrol";
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with very long string
 */
TEST_F(JsonUtilsTest, EscapeJson_LongString) {
    std::string input(1000, 'A');
    std::string expected(1000, 'A');
    EXPECT_EQ(escapeJson(input), expected);
}

/**
 * Test: escapeJson with only special characters
 */
TEST_F(JsonUtilsTest, EscapeJson_OnlySpecialCharacters) {
    std::string input = "\"\\\n\t\r";
    std::string expected = "\\\"\\\\\\n\\t\\r";
    EXPECT_EQ(escapeJson(input), expected);
}
