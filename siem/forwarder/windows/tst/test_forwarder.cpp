/**
 * @file test_forwarder.cpp
 * @brief Test program to verify Windows Event Log Forwarder functionality
 *
 * This test acts as a mock SIEM server that listens on port 8089 and receives
 * forwarded logs. It verifies that:
 * 1. The forwarder can connect to the server
 * 2. Logs are properly formatted as JSON
 * 3. Required fields are present in the log data
 *
 * Usage:
 *   1. Run this test program first (it acts as the SIEM server)
 *   2. Run the log_forwarder.exe to connect and forward logs
 *   3. This program will display received logs and validate them
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

const int TEST_PORT = 8089;
const int BUFFER_SIZE = 4096;

/**
 * @brief Validate that a received log is properly formatted JSON
 * @param logData The log string to validate
 * @return true if valid, false otherwise
 */
bool validateLogFormat(const std::string& logData) {
    // Basic JSON validation - check for required fields
    bool hasEventId = logData.find("\"event_id\"") != std::string::npos;
    bool hasLevel = logData.find("\"level\"") != std::string::npos;
    bool hasChannel = logData.find("\"channel\"") != std::string::npos;
    bool hasComputer = logData.find("\"computer\"") != std::string::npos;
    bool hasTimestamp = logData.find("\"timestamp\"") != std::string::npos;

    // Check JSON structure
    bool hasOpenBrace = logData.find("{") != std::string::npos;
    bool hasCloseBrace = logData.find("}") != std::string::npos;

    return hasEventId && hasLevel && hasChannel && hasComputer &&
           hasTimestamp && hasOpenBrace && hasCloseBrace;
}

/**
 * @brief Handle incoming connection from log forwarder
 * @param clientSocket Socket connected to the forwarder
 */
void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string receivedData;
    int logsReceived = 0;
    int validLogs = 0;
    int invalidLogs = 0;

    std::cout << "[TEST] Client connected" << std::endl;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            receivedData += buffer;

            // Process complete lines (JSON objects)
            size_t pos;
            while ((pos = receivedData.find('\n')) != std::string::npos) {
                std::string logLine = receivedData.substr(0, pos);
                receivedData.erase(0, pos + 1);

                if (!logLine.empty()) {
                    logsReceived++;

                    std::cout << "\n[TEST] Received Log #" << logsReceived << ":" << std::endl;
                    std::cout << logLine << std::endl;

                    // Validate log format
                    if (validateLogFormat(logLine)) {
                        validLogs++;
                        std::cout << "[TEST] ✓ Log validation PASSED" << std::endl;
                    } else {
                        invalidLogs++;
                        std::cout << "[TEST] ✗ Log validation FAILED - Missing required fields" << std::endl;
                    }

                    std::cout << "[TEST] Statistics: " << logsReceived << " total, "
                              << validLogs << " valid, " << invalidLogs << " invalid" << std::endl;
                }
            }
        } else if (bytesReceived == 0) {
            std::cout << "\n[TEST] Client disconnected" << std::endl;
            break;
        } else {
            std::cout << "[TEST] recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    closesocket(clientSocket);

    // Print final results
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST RESULTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total logs received: " << logsReceived << std::endl;
    std::cout << "Valid logs: " << validLogs << std::endl;
    std::cout << "Invalid logs: " << invalidLogs << std::endl;

    if (logsReceived > 0 && invalidLogs == 0) {
        std::cout << "\n[TEST] ✓✓✓ ALL TESTS PASSED ✓✓✓" << std::endl;
    } else if (logsReceived == 0) {
        std::cout << "\n[TEST] ⚠ WARNING: No logs received" << std::endl;
    } else {
        std::cout << "\n[TEST] ✗✗✗ SOME TESTS FAILED ✗✗✗" << std::endl;
    }
    std::cout << "========================================" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Windows Event Log Forwarder Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "This program acts as a mock SIEM server" << std::endl;
    std::cout << "Listening on port: " << TEST_PORT << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";

    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[TEST] WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // Create socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "[TEST] socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(TEST_PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[TEST] bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[TEST] listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[TEST] Mock SIEM server started successfully" << std::endl;
    std::cout << "[TEST] Waiting for log forwarder to connect..." << std::endl;
    std::cout << "\n>> Now run: log_forwarder.exe\n" << std::endl;

    // Accept connection
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "[TEST] accept failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Handle client and run tests
    handleClient(clientSocket);

    // Cleanup
    closesocket(listenSocket);
    WSACleanup();

    std::cout << "\n[TEST] Test server shutting down..." << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
