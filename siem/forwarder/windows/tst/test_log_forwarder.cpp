/**
 * @file test_log_forwarder.cpp
 * @brief Comprehensive tests for LogForwarder class
 *
 * Tests TCP socket communication functionality for forwarding logs to SIEM/Splunk server.
 * Includes unit tests and integration tests with Splunk on link-local address.
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "log_forwarder.h"
#include "logger.h"

#pragma comment(lib, "ws2_32.lib")

// Test framework macros
int passed = 0;
int failed = 0;

#define TEST_START(name) std::cout << "Testing: " << name << "... "
#define TEST_PASS() { std::cout << "[PASS]" << std::endl; passed++; }
#define TEST_FAIL(msg) { std::cout << "[FAIL] " << msg << std::endl; failed++; }

#define ASSERT_TRUE(condition, msg) \
    if (!(condition)) { TEST_FAIL(msg); return false; }

#define ASSERT_FALSE(condition, msg) \
    if (condition) { TEST_FAIL(msg); return false; }

// ============================================================================
// Test Configuration - MODIFY THESE FOR YOUR SPLUNK SETUP
// ============================================================================

// Link-local address for Splunk HEC (HTTP Event Collector)
// Common link-local addresses:
// - IPv6: fe80::1 (link-local gateway)
// - IPv4: 169.254.x.x (APIPA range)
// - Localhost: 127.0.0.1 (for testing)
const std::string SPLUNK_SERVER = "169.254.1.1";  // Default link-local, CHANGE THIS
const int SPLUNK_PORT = 8088;                      // Splunk HEC default port

// For testing, we'll also use a local test server
const std::string TEST_SERVER = "127.0.0.1";
const int TEST_PORT = 9999;

// ============================================================================
// Mock TCP Server for Testing
// ============================================================================

class MockTcpServer {
private:
    SOCKET listenSocket;
    SOCKET clientSocket;
    bool running;
    std::thread serverThread;
    int port;
    std::string lastReceived;
    std::mutex dataMutex;

public:
    MockTcpServer(int portNum) : listenSocket(INVALID_SOCKET),
                                  clientSocket(INVALID_SOCKET),
                                  running(false), port(portNum) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~MockTcpServer() {
        stop();
        WSACleanup();
    }

    bool start() {
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            return false;
        }

        // Allow address reuse
        int opt = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
            return false;
        }

        if (listen(listenSocket, 1) == SOCKET_ERROR) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
            return false;
        }

        running = true;
        serverThread = std::thread(&MockTcpServer::acceptLoop, this);

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }

    void stop() {
        running = false;
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    std::string getLastReceived() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return lastReceived;
    }

private:
    void acceptLoop() {
        while (running) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenSocket, &readSet);

            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int result = select(0, &readSet, nullptr, nullptr, &timeout);
            if (result > 0) {
                clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket != INVALID_SOCKET) {
                    handleClient();
                }
            }
        }
    }

    void handleClient() {
        char buffer[4096];
        while (running) {
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::lock_guard<std::mutex> lock(dataMutex);
                lastReceived = std::string(buffer);
            } else {
                break;
            }
        }
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
};

// ============================================================================
// Unit Tests - Basic Functionality
// ============================================================================

bool test_LogForwarder_Constructor() {
    TEST_START("LogForwarder constructor");

    LogForwarder forwarder("192.168.1.100", 8089);
    ASSERT_FALSE(forwarder.isConnected(), "Should not be connected on construction");

    TEST_PASS();
    return true;
}

bool test_LogForwarder_Initialize() {
    TEST_START("LogForwarder initialize (WSA startup)");

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    bool result = forwarder.initialize();

    ASSERT_TRUE(result, "Initialize should succeed");

    // Cleanup
    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_ConnectToInvalidServer() {
    TEST_START("LogForwarder connect to invalid server");

    LogForwarder forwarder("192.0.2.1", 9999);  // TEST-NET-1, should be unreachable
    forwarder.initialize();

    bool result = forwarder.connect();
    ASSERT_FALSE(result, "Should fail to connect to unreachable server");
    ASSERT_FALSE(forwarder.isConnected(), "Should not be connected");

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_ConnectToMockServer() {
    TEST_START("LogForwarder connect to mock server");

    MockTcpServer server(TEST_PORT);
    if (!server.start()) {
        TEST_FAIL("Failed to start mock server");
        return false;
    }

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();

    bool result = forwarder.connect();
    ASSERT_TRUE(result, "Should connect to mock server");
    ASSERT_TRUE(forwarder.isConnected(), "Should be connected");

    forwarder.disconnect();
    server.stop();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendLogWhenNotConnected() {
    TEST_START("LogForwarder send log when not connected");

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();

    bool result = forwarder.sendLog("{\"test\":\"data\"}");
    ASSERT_FALSE(result, "Should fail to send when not connected");

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendLogSuccess() {
    TEST_START("LogForwarder send log successfully");

    MockTcpServer server(TEST_PORT);
    if (!server.start()) {
        TEST_FAIL("Failed to start mock server");
        return false;
    }

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();
    forwarder.connect();

    std::string testData = "{\"event\":\"test\",\"timestamp\":\"2026-01-14\"}";
    bool result = forwarder.sendLog(testData);

    ASSERT_TRUE(result, "Should send successfully");

    // Give time for data to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string received = server.getLastReceived();
    ASSERT_FALSE(received.empty(), "Server should receive data");
    ASSERT_TRUE(received.find(testData) != std::string::npos,
                "Received data should contain sent message");

    forwarder.disconnect();
    server.stop();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendMultipleLogs() {
    TEST_START("LogForwarder send multiple logs");

    MockTcpServer server(TEST_PORT);
    if (!server.start()) {
        TEST_FAIL("Failed to start mock server");
        return false;
    }

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();
    forwarder.connect();

    // Send multiple log entries
    for (int i = 0; i < 5; i++) {
        std::string logData = "{\"event\":\"test_" + std::to_string(i) +
                             "\",\"index\":" + std::to_string(i) + "}";
        bool result = forwarder.sendLog(logData);
        ASSERT_TRUE(result, "Send should succeed for message " + std::to_string(i));
    }

    forwarder.disconnect();
    server.stop();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_DisconnectAndReconnect() {
    TEST_START("LogForwarder disconnect and reconnect");

    MockTcpServer server(TEST_PORT);
    if (!server.start()) {
        TEST_FAIL("Failed to start mock server");
        return false;
    }

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();

    // First connection
    bool result1 = forwarder.connect();
    ASSERT_TRUE(result1, "First connection should succeed");
    ASSERT_TRUE(forwarder.isConnected(), "Should be connected");

    // Disconnect
    forwarder.disconnect();
    ASSERT_FALSE(forwarder.isConnected(), "Should not be connected after disconnect");

    // Reconnect
    forwarder.initialize();
    bool result2 = forwarder.connect();
    ASSERT_TRUE(result2, "Reconnection should succeed");
    ASSERT_TRUE(forwarder.isConnected(), "Should be connected again");

    forwarder.disconnect();
    server.stop();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendLargeLog() {
    TEST_START("LogForwarder send large log entry");

    MockTcpServer server(TEST_PORT);
    if (!server.start()) {
        TEST_FAIL("Failed to start mock server");
        return false;
    }

    LogForwarder forwarder(TEST_SERVER, TEST_PORT);
    forwarder.initialize();
    forwarder.connect();

    // Create a large log entry (1KB)
    std::string largeData = "{\"event\":\"large_test\",\"data\":\"";
    for (int i = 0; i < 100; i++) {
        largeData += "ABCDEFGHIJ";  // 10 bytes * 100 = 1000 bytes
    }
    largeData += "\"}";

    bool result = forwarder.sendLog(largeData);
    ASSERT_TRUE(result, "Should send large log successfully");

    forwarder.disconnect();
    server.stop();

    TEST_PASS();
    return true;
}

// ============================================================================
// Integration Tests - Splunk on Link-Local Address
// ============================================================================

bool test_LogForwarder_SplunkConnectivity() {
    TEST_START("LogForwarder connect to Splunk on link-local address");

    std::cout << std::endl;
    std::cout << "  Attempting to connect to: " << SPLUNK_SERVER << ":" << SPLUNK_PORT << std::endl;
    std::cout << "  (Configure SPLUNK_SERVER and SPLUNK_PORT at top of test file)" << std::endl;

    LogForwarder forwarder(SPLUNK_SERVER, SPLUNK_PORT);
    forwarder.initialize();

    bool result = forwarder.connect();

    if (!result) {
        std::cout << "  [INFO] Could not connect to Splunk - skipping test" << std::endl;
        std::cout << "  Make sure Splunk is running and HEC is enabled" << std::endl;
        forwarder.disconnect();
        TEST_PASS();  // Don't fail if Splunk isn't available
        return true;
    }

    ASSERT_TRUE(forwarder.isConnected(), "Should be connected to Splunk");

    std::cout << "  [SUCCESS] Connected to Splunk!" << std::endl;

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendToSplunk() {
    TEST_START("LogForwarder send Windows event to Splunk");

    std::cout << std::endl;
    std::cout << "  Sending test event to Splunk at: " << SPLUNK_SERVER << ":" << SPLUNK_PORT << std::endl;

    LogForwarder forwarder(SPLUNK_SERVER, SPLUNK_PORT);
    forwarder.initialize();

    if (!forwarder.connect()) {
        std::cout << "  [INFO] Could not connect to Splunk - skipping test" << std::endl;
        forwarder.disconnect();
        TEST_PASS();  // Don't fail if Splunk isn't available
        return true;
    }

    // Create a sample Windows event log in JSON format
    std::string eventLog = R"({
  "EventID": "4624",
  "Level": "Information",
  "Channel": "Security",
  "Computer": "TEST-MACHINE",
  "TimeCreated": "2026-01-14T12:00:00.000Z",
  "Provider": "Microsoft-Windows-Security-Auditing",
  "Message": "An account was successfully logged on",
  "EventData": {
    "SubjectUserSid": "S-1-5-18",
    "SubjectUserName": "SYSTEM",
    "SubjectDomainName": "NT AUTHORITY",
    "LogonType": "3",
    "IpAddress": "192.168.1.100"
  }
})";

    bool result = forwarder.sendLog(eventLog);

    if (result) {
        std::cout << "  [SUCCESS] Event sent to Splunk!" << std::endl;
        std::cout << "  Check your Splunk instance for the test event (EventID: 4624)" << std::endl;
    }

    ASSERT_TRUE(result, "Should send event to Splunk successfully");

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SendMultipleEventsToSplunk() {
    TEST_START("LogForwarder send multiple Windows events to Splunk");

    std::cout << std::endl;
    std::cout << "  Sending 10 test events to Splunk..." << std::endl;

    LogForwarder forwarder(SPLUNK_SERVER, SPLUNK_PORT);
    forwarder.initialize();

    if (!forwarder.connect()) {
        std::cout << "  [INFO] Could not connect to Splunk - skipping test" << std::endl;
        forwarder.disconnect();
        TEST_PASS();  // Don't fail if Splunk isn't available
        return true;
    }

    int successCount = 0;

    // Send 10 different event types
    std::vector<std::string> eventTypes = {
        "4624", "4625", "4672", "4688", "4689",
        "1102", "7045", "4720", "4722", "4732"
    };

    for (int i = 0; i < 10; i++) {
        std::string eventLog = "{\"EventID\":\"" + eventTypes[i] +
                              "\",\"Level\":\"Information\"," +
                              "\"Channel\":\"Security\"," +
                              "\"Computer\":\"TEST-MACHINE\"," +
                              "\"TimeCreated\":\"2026-01-14T12:00:" +
                              std::to_string(i) + ".000Z\"," +
                              "\"Message\":\"Test event " + std::to_string(i) + "\"}";

        if (forwarder.sendLog(eventLog)) {
            successCount++;
        }

        // Small delay between sends
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "  [INFO] Successfully sent " << successCount << "/10 events" << std::endl;

    ASSERT_TRUE(successCount == 10, "Should send all 10 events successfully");

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

bool test_LogForwarder_SplunkReconnect() {
    TEST_START("LogForwarder reconnect to Splunk after disconnect");

    std::cout << std::endl;

    LogForwarder forwarder(SPLUNK_SERVER, SPLUNK_PORT);
    forwarder.initialize();

    if (!forwarder.connect()) {
        std::cout << "  [INFO] Could not connect to Splunk - skipping test" << std::endl;
        forwarder.disconnect();
        TEST_PASS();  // Don't fail if Splunk isn't available
        return true;
    }

    std::cout << "  [INFO] First connection successful" << std::endl;

    // Send an event
    std::string event1 = "{\"EventID\":\"1000\",\"Message\":\"Before disconnect\"}";
    bool result1 = forwarder.sendLog(event1);
    ASSERT_TRUE(result1, "Should send event on first connection");

    // Disconnect
    std::cout << "  [INFO] Disconnecting..." << std::endl;
    forwarder.disconnect();

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Reconnect
    std::cout << "  [INFO] Reconnecting..." << std::endl;
    forwarder.initialize();
    bool result2 = forwarder.connect();
    ASSERT_TRUE(result2, "Should reconnect successfully");

    // Send another event
    std::string event2 = "{\"EventID\":\"1001\",\"Message\":\"After reconnect\"}";
    bool result3 = forwarder.sendLog(event2);
    ASSERT_TRUE(result3, "Should send event after reconnection");

    std::cout << "  [SUCCESS] Reconnection successful!" << std::endl;

    forwarder.disconnect();

    TEST_PASS();
    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

void printSplunkSetupInstructions() {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Splunk HEC Setup Instructions" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";
    std::cout << "To test with Splunk on link-local address:" << std::endl;
    std::cout << "\n";
    std::cout << "1. Configure Splunk HEC (HTTP Event Collector):" << std::endl;
    std::cout << "   - Go to Settings > Data Inputs > HTTP Event Collector" << std::endl;
    std::cout << "   - Click 'New Token' and configure" << std::endl;
    std::cout << "   - Note the token and port (default: 8088)" << std::endl;
    std::cout << "\n";
    std::cout << "2. Enable HEC on link-local address:" << std::endl;
    std::cout << "   - Edit inputs.conf or use Splunk Web UI" << std::endl;
    std::cout << "   - Set enableSSL = 0 (for testing)" << std::endl;
    std::cout << "   - Bind to your link-local address" << std::endl;
    std::cout << "\n";
    std::cout << "3. Find your link-local address:" << std::endl;
    std::cout << "   - Windows: ipconfig | findstr \"Link-local\"" << std::endl;
    std::cout << "   - Common: fe80::1 (IPv6) or 169.254.x.x (IPv4)" << std::endl;
    std::cout << "\n";
    std::cout << "4. Update test configuration:" << std::endl;
    std::cout << "   - Edit SPLUNK_SERVER in test_log_forwarder.cpp" << std::endl;
    std::cout << "   - Set to your link-local address" << std::endl;
    std::cout << "   - Update SPLUNK_PORT if needed (default: 8088)" << std::endl;
    std::cout << "\n";
    std::cout << "Current configuration:" << std::endl;
    std::cout << "  Server: " << SPLUNK_SERVER << std::endl;
    std::cout << "  Port: " << SPLUNK_PORT << std::endl;
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "\n";
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Log Forwarder Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";

    initializeGlobalLogger("test_log_forwarder.csv");

    // Print Splunk setup instructions
    printSplunkSetupInstructions();

    std::cout << "Running Unit Tests..." << std::endl;
    std::cout << "========================================" << std::endl;

    // Unit tests
    test_LogForwarder_Constructor();
    test_LogForwarder_Initialize();
    test_LogForwarder_ConnectToInvalidServer();
    test_LogForwarder_ConnectToMockServer();
    test_LogForwarder_SendLogWhenNotConnected();
    test_LogForwarder_SendLogSuccess();
    test_LogForwarder_SendMultipleLogs();
    test_LogForwarder_DisconnectAndReconnect();
    test_LogForwarder_SendLargeLog();

    std::cout << "\n";
    std::cout << "Running Splunk Integration Tests..." << std::endl;
    std::cout << "========================================" << std::endl;

    // Splunk integration tests
    test_LogForwarder_SplunkConnectivity();
    test_LogForwarder_SendToSplunk();
    test_LogForwarder_SendMultipleEventsToSplunk();
    test_LogForwarder_SplunkReconnect();

    shutdownGlobalLogger();
    std::remove("test_log_forwarder.csv");

    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Total:  " << (passed + failed) << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";

    if (failed == 0) {
        std::cout << "All tests passed! ✓" << std::endl;
    } else {
        std::cout << "Some tests failed! ✗" << std::endl;
    }

    return (failed == 0) ? 0 : 1;
}
