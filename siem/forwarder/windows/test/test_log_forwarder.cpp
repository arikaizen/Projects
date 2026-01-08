/**
 * @file test_log_forwarder.cpp
 * @brief Unit/Integration tests for LogForwarder
 *
 * Tests TCP socket communication functionality
 * Note: Some tests require network access or mock server
 */

#include <gtest/gtest.h>
#include "../inc/log_forwarder.h"
#include "../inc/logger.h"
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

// Mock SIEM server for testing
class MockSIEMServer {
private:
    SOCKET listenSocket;
    SOCKET clientSocket;
    int port;
    bool running;
    std::thread serverThread;
    std::string receivedData;

public:
    MockSIEMServer(int serverPort) : port(serverPort), running(false),
                                      listenSocket(INVALID_SOCKET),
                                      clientSocket(INVALID_SOCKET) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~MockSIEMServer() {
        stop();
        WSACleanup();
    }

    bool start() {
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(listenSocket);
            return false;
        }

        if (listen(listenSocket, 1) == SOCKET_ERROR) {
            closesocket(listenSocket);
            return false;
        }

        running = true;
        serverThread = std::thread(&MockSIEMServer::acceptLoop, this);

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

    void acceptLoop() {
        while (running) {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(listenSocket, &readSet);

            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms

            int result = select(0, &readSet, nullptr, nullptr, &timeout);
            if (result > 0) {
                clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket != INVALID_SOCKET) {
                    receiveData();
                }
            }
        }
    }

    void receiveData() {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            receivedData += std::string(buffer);
        }
    }

    std::string getReceivedData() const {
        return receivedData;
    }

    void clearReceivedData() {
        receivedData.clear();
    }
};

// Test fixture for LogForwarder tests
class LogForwarderTest : public ::testing::Test {
protected:
    LogForwarder* forwarder;
    MockSIEMServer* mockServer;
    int testPort;

    void SetUp() override {
        // Use a high port number for testing
        testPort = 19999;
        forwarder = nullptr;
        mockServer = nullptr;

        // Initialize a test logger to suppress logger errors
        initializeGlobalLogger("test_forwarder.csv");
    }

    void TearDown() override {
        if (forwarder != nullptr) {
            forwarder->disconnect();
            delete forwarder;
            forwarder = nullptr;
        }
        if (mockServer != nullptr) {
            mockServer->stop();
            delete mockServer;
            mockServer = nullptr;
        }

        shutdownGlobalLogger();

        // Clean up test log file
        std::remove("test_forwarder.csv");
    }
};

/**
 * Test: Constructor creates LogForwarder instance
 */
TEST_F(LogForwarderTest, Constructor_CreatesInstance) {
    forwarder = new LogForwarder("127.0.0.1", testPort);
    EXPECT_NE(forwarder, nullptr);
}

/**
 * Test: Initialize succeeds with WSAStartup
 */
TEST_F(LogForwarderTest, Initialize_Succeeds) {
    forwarder = new LogForwarder("127.0.0.1", testPort);
    EXPECT_TRUE(forwarder->initialize());
}

/**
 * Test: Initial connection state is false
 */
TEST_F(LogForwarderTest, InitialState_NotConnected) {
    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    EXPECT_FALSE(forwarder->isConnected());
}

/**
 * Test: Connect to mock server succeeds
 */
TEST_F(LogForwarderTest, Connect_ToMockServer_Succeeds) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();

    EXPECT_TRUE(forwarder->connect());
    EXPECT_TRUE(forwarder->isConnected());
}

/**
 * Test: Connect fails when no server is listening
 */
TEST_F(LogForwarderTest, Connect_NoServer_Fails) {
    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();

    EXPECT_FALSE(forwarder->connect());
    EXPECT_FALSE(forwarder->isConnected());
}

/**
 * Test: Connect with invalid hostname fails
 */
TEST_F(LogForwarderTest, Connect_InvalidHostname_Fails) {
    forwarder = new LogForwarder("invalid.hostname.that.does.not.exist", testPort);
    forwarder->initialize();

    EXPECT_FALSE(forwarder->connect());
    EXPECT_FALSE(forwarder->isConnected());
}

/**
 * Test: SendLog succeeds when connected
 */
TEST_F(LogForwarderTest, SendLog_WhenConnected_Succeeds) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    std::string testMessage = "{\"test\": \"data\"}";
    EXPECT_TRUE(forwarder->sendLog(testMessage));

    // Give time for data to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string received = mockServer->getReceivedData();
    EXPECT_TRUE(received.find(testMessage) != std::string::npos);
}

/**
 * Test: SendLog fails when not connected
 */
TEST_F(LogForwarderTest, SendLog_WhenNotConnected_Fails) {
    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();

    std::string testMessage = "{\"test\": \"data\"}";
    EXPECT_FALSE(forwarder->sendLog(testMessage));
}

/**
 * Test: SendLog appends newline
 */
TEST_F(LogForwarderTest, SendLog_AppendsNewline) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    std::string testMessage = "{\"test\": \"data\"}";
    forwarder->sendLog(testMessage);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string received = mockServer->getReceivedData();
    EXPECT_TRUE(received.find(testMessage + "\n") != std::string::npos);
}

/**
 * Test: Multiple SendLog operations
 */
TEST_F(LogForwarderTest, SendLog_Multiple_AllSucceed) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    EXPECT_TRUE(forwarder->sendLog("{\"msg\": 1}"));
    EXPECT_TRUE(forwarder->sendLog("{\"msg\": 2}"));
    EXPECT_TRUE(forwarder->sendLog("{\"msg\": 3}"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string received = mockServer->getReceivedData();
    EXPECT_TRUE(received.find("{\"msg\": 1}") != std::string::npos);
    EXPECT_TRUE(received.find("{\"msg\": 2}") != std::string::npos);
    EXPECT_TRUE(received.find("{\"msg\": 3}") != std::string::npos);
}

/**
 * Test: Disconnect closes connection
 */
TEST_F(LogForwarderTest, Disconnect_ClosesConnection) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    EXPECT_TRUE(forwarder->isConnected());

    forwarder->disconnect();

    EXPECT_FALSE(forwarder->isConnected());
}

/**
 * Test: SendLog after disconnect fails
 */
TEST_F(LogForwarderTest, SendLog_AfterDisconnect_Fails) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();
    forwarder->disconnect();

    EXPECT_FALSE(forwarder->sendLog("{\"test\": \"data\"}"));
}

/**
 * Test: Reconnect after disconnect succeeds
 */
TEST_F(LogForwarderTest, Reconnect_AfterDisconnect_Succeeds) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();
    forwarder->disconnect();

    // Reconnect
    EXPECT_TRUE(forwarder->connect());
    EXPECT_TRUE(forwarder->isConnected());
}

/**
 * Test: SendLog with empty string
 */
TEST_F(LogForwarderTest, SendLog_EmptyString_Succeeds) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    EXPECT_TRUE(forwarder->sendLog(""));
}

/**
 * Test: SendLog with large data
 */
TEST_F(LogForwarderTest, SendLog_LargeData_Succeeds) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    std::string largeMessage(10000, 'A');
    EXPECT_TRUE(forwarder->sendLog(largeMessage));
}

/**
 * Test: Destructor calls disconnect
 */
TEST_F(LogForwarderTest, Destructor_CallsDisconnect) {
    mockServer = new MockSIEMServer(testPort);
    ASSERT_TRUE(mockServer->start());

    forwarder = new LogForwarder("127.0.0.1", testPort);
    forwarder->initialize();
    forwarder->connect();

    EXPECT_TRUE(forwarder->isConnected());

    delete forwarder;
    forwarder = nullptr;

    // If destructor didn't cleanup, we might have issues
    // This test mainly checks for no crashes/leaks
    SUCCEED();
}
