/**
 * @file test_logger.cpp
 * @brief Unit tests for CSV Logger
 *
 * Tests logging functionality, CSV formatting, and file operations
 */

#include <gtest/gtest.h>
#include "../inc/logger.h"
#include <fstream>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Test fixture for Logger tests
class LoggerTest : public ::testing::Test {
protected:
    std::string testLogFile;
    Logger* logger;

    void SetUp() override {
        // Create unique test log file name
        testLogFile = "test_log_" + std::to_string(std::time(nullptr)) + ".csv";
        logger = nullptr;
    }

    void TearDown() override {
        // Cleanup
        if (logger != nullptr) {
            delete logger;
            logger = nullptr;
        }

        // Remove test log file
        if (fs::exists(testLogFile)) {
            fs::remove(testLogFile);
        }
    }

    // Helper to read log file contents
    std::string readLogFile() {
        std::ifstream file(testLogFile);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Helper to count lines in log file
    int countLines() {
        std::ifstream file(testLogFile);
        int count = 0;
        std::string line;
        while (std::getline(file, line)) {
            count++;
        }
        return count;
    }
};

/**
 * Test: Logger initialization creates file with CSV header
 */
TEST_F(LoggerTest, Initialize_CreatesFileWithHeader) {
    logger = new Logger(testLogFile);
    ASSERT_TRUE(logger->initialize());
    EXPECT_TRUE(logger->isReady());

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Timestamp,Level,Component,Message,Details") != std::string::npos);
}

/**
 * Test: Logger initialization fails with invalid path
 */
TEST_F(LoggerTest, Initialize_FailsWithInvalidPath) {
    logger = new Logger("Z:\\invalid\\path\\log.csv");
    EXPECT_FALSE(logger->initialize());
    EXPECT_FALSE(logger->isReady());
}

/**
 * Test: Log INFO message
 */
TEST_F(LoggerTest, LogInfo_WritesCorrectFormat) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("TestComponent", "Test message", "Test details");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
    EXPECT_TRUE(content.find("TestComponent") != std::string::npos);
    EXPECT_TRUE(content.find("Test message") != std::string::npos);
    EXPECT_TRUE(content.find("Test details") != std::string::npos);
}

/**
 * Test: Log WARNING message
 */
TEST_F(LoggerTest, LogWarning_WritesCorrectLevel) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->warning("Component", "Warning message", "");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("WARNING") != std::string::npos);
}

/**
 * Test: Log ERROR message
 */
TEST_F(LoggerTest, LogError_WritesCorrectLevel) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->error("Component", "Error message", "");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("ERROR") != std::string::npos);
}

/**
 * Test: Log DEBUG message
 */
TEST_F(LoggerTest, LogDebug_WritesCorrectLevel) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->debug("Component", "Debug message", "");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("DEBUG") != std::string::npos);
}

/**
 * Test: CSV escaping with commas
 */
TEST_F(LoggerTest, CsvEscape_HandlesCommas) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Message with, comma", "Details, with, commas");

    std::string content = readLogFile();
    // Fields with commas should be quoted
    EXPECT_TRUE(content.find("\"Message with, comma\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"Details, with, commas\"") != std::string::npos);
}

/**
 * Test: CSV escaping with quotes
 */
TEST_F(LoggerTest, CsvEscape_HandlesQuotes) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Message with \"quotes\"", "");

    std::string content = readLogFile();
    // Quotes should be doubled and field quoted
    EXPECT_TRUE(content.find("\"Message with \"\"quotes\"\"\"") != std::string::npos);
}

/**
 * Test: CSV escaping with newlines
 */
TEST_F(LoggerTest, CsvEscape_HandlesNewlines) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Line1\nLine2", "");

    std::string content = readLogFile();
    // Fields with newlines should be quoted
    EXPECT_TRUE(content.find("\"Line1\nLine2\"") != std::string::npos);
}

/**
 * Test: Multiple log entries create multiple lines
 */
TEST_F(LoggerTest, MultipleLogs_CreateMultipleLines) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("C1", "M1", "D1");
    logger->warning("C2", "M2", "D2");
    logger->error("C3", "M3", "D3");

    // Header + 3 log lines = 4 total lines
    EXPECT_EQ(countLines(), 4);
}

/**
 * Test: Timestamp format is correct
 */
TEST_F(LoggerTest, Timestamp_CorrectFormat) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Message", "");

    std::string content = readLogFile();
    // Should contain timestamp in format YYYY-MM-DD HH:MM:SS
    // Using regex would be better, but checking for basic format
    EXPECT_TRUE(content.find("-") != std::string::npos); // Date separators
    EXPECT_TRUE(content.find(":") != std::string::npos); // Time separators
}

/**
 * Test: Flush operation
 */
TEST_F(LoggerTest, Flush_WritesToDisk) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Message", "");
    logger->flush();

    // File should exist and contain the log
    EXPECT_TRUE(fs::exists(testLogFile));
    EXPECT_GT(fs::file_size(testLogFile), 0);
}

/**
 * Test: Logging without initialization does nothing
 */
TEST_F(LoggerTest, LogWithoutInit_DoesNothing) {
    logger = new Logger(testLogFile);
    // Do not initialize

    logger->info("Component", "Message", "");

    // File should not exist
    EXPECT_FALSE(fs::exists(testLogFile));
}

/**
 * Test: Thread safety - concurrent logging
 */
TEST_F(LoggerTest, ThreadSafety_ConcurrentLogging) {
    logger = new Logger(testLogFile);
    logger->initialize();

    const int numThreads = 10;
    const int logsPerThread = 100;
    std::vector<std::thread> threads;

    // Create threads that log concurrently
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([this, i, logsPerThread]() {
            for (int j = 0; j < logsPerThread; j++) {
                logger->info("Thread" + std::to_string(i),
                           "Message" + std::to_string(j), "");
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Should have header + (numThreads * logsPerThread) log lines
    EXPECT_EQ(countLines(), 1 + (numThreads * logsPerThread));
}

/**
 * Test: Empty details field
 */
TEST_F(LoggerTest, EmptyDetails_HandledCorrectly) {
    logger = new Logger(testLogFile);
    logger->initialize();

    logger->info("Component", "Message", "");

    std::string content = readLogFile();
    // Should have 5 fields separated by 4 commas (empty last field)
    int commaCount = 0;
    for (char c : content) {
        if (c == ',') commaCount++;
    }
    EXPECT_GE(commaCount, 4); // At least 4 commas for CSV format
}

/**
 * Test: Append mode - existing file preserves old logs
 */
TEST_F(LoggerTest, AppendMode_PreservesOldLogs) {
    // Create first logger and write
    logger = new Logger(testLogFile);
    logger->initialize();
    logger->info("Component1", "Message1", "");
    delete logger;

    // Create second logger (should append)
    logger = new Logger(testLogFile);
    logger->initialize();
    logger->info("Component2", "Message2", "");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Message1") != std::string::npos);
    EXPECT_TRUE(content.find("Message2") != std::string::npos);

    // Header should only appear once
    int headerCount = 0;
    size_t pos = 0;
    std::string header = "Timestamp,Level,Component,Message,Details";
    while ((pos = content.find(header, pos)) != std::string::npos) {
        headerCount++;
        pos += header.length();
    }
    EXPECT_EQ(headerCount, 1);
}

/**
 * Test: Global logger initialization
 */
TEST_F(LoggerTest, GlobalLogger_Initialize) {
    EXPECT_TRUE(initializeGlobalLogger(testLogFile));
    EXPECT_NE(g_logger, nullptr);

    shutdownGlobalLogger();
    EXPECT_EQ(g_logger, nullptr);
}

/**
 * Test: Global logger usage
 */
TEST_F(LoggerTest, GlobalLogger_Usage) {
    initializeGlobalLogger(testLogFile);

    g_logger->info("Global", "Test message", "");

    std::string content = readLogFile();
    EXPECT_TRUE(content.find("Global") != std::string::npos);
    EXPECT_TRUE(content.find("Test message") != std::string::npos);

    shutdownGlobalLogger();
}

/**
 * Test: Global logger re-initialization
 */
TEST_F(LoggerTest, GlobalLogger_Reinitialize) {
    initializeGlobalLogger(testLogFile);
    Logger* firstInstance = g_logger;

    // Re-initialize should replace old instance
    initializeGlobalLogger(testLogFile);
    EXPECT_NE(g_logger, nullptr);

    shutdownGlobalLogger();
}
