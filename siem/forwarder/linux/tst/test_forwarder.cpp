/**
 * @file test_forwarder.cpp
 * @brief Test program to verify Linux System Log Forwarder functionality
 *
 * This test acts as a mock SIEM server that listens on port 8089 and receives
 * forwarded logs. It verifies that:
 * 1. The forwarder can connect to the server
 * 2. Logs are properly formatted as JSON
 * 3. Required fields are present in the log data
 *
 * Usage:
 *   1. Run this test program first (it acts as the SIEM server)
 *   2. Run ./log_forwarder to connect and forward logs
 *   3. This program will display received logs and validate them
 */

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int TEST_PORT = 8089;
const int BUFFER_SIZE = 4096;

/**
 * @brief Validate that a received log is properly formatted JSON
 * @param logData The log string to validate
 * @return true if valid, false otherwise
 */
bool validateLogFormat(const std::string& logData) {
    // Basic JSON validation - check for required fields
    bool hasMessage = logData.find("\"message\"") != std::string::npos;
    bool hasPriority = logData.find("\"priority\"") != std::string::npos;
    bool hasUnit = logData.find("\"unit\"") != std::string::npos;
    bool hasHostname = logData.find("\"hostname\"") != std::string::npos;
    bool hasTimestamp = logData.find("\"timestamp\"") != std::string::npos;

    // Check JSON structure
    bool hasOpenBrace = logData.find("{") != std::string::npos;
    bool hasCloseBrace = logData.find("}") != std::string::npos;

    return hasMessage && hasPriority && hasUnit && hasHostname &&
           hasTimestamp && hasOpenBrace && hasCloseBrace;
}

/**
 * @brief Handle incoming connection from log forwarder
 * @param clientSocket Socket connected to the forwarder
 */
void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string receivedData;
    int logsReceived = 0;
    int validLogs = 0;
    int invalidLogs = 0;

    std::cout << "[TEST] Client connected" << std::endl;

    while (true) {
        ssize_t bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

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
            std::cerr << "[TEST] recv failed: " << strerror(errno) << std::endl;
            break;
        }
    }

    close(clientSocket);

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
    std::cout << "Linux System Log Forwarder Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "This program acts as a mock SIEM server" << std::endl;
    std::cout << "Listening on port: " << TEST_PORT << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";

    // Create socket
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        std::cerr << "[TEST] socket failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "[TEST] setsockopt failed: " << strerror(errno) << std::endl;
        close(listenSocket);
        return 1;
    }

    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(TEST_PORT);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "[TEST] bind failed: " << strerror(errno) << std::endl;
        close(listenSocket);
        return 1;
    }

    // Listen for connections
    if (listen(listenSocket, 5) == -1) {
        std::cerr << "[TEST] listen failed: " << strerror(errno) << std::endl;
        close(listenSocket);
        return 1;
    }

    std::cout << "[TEST] Mock SIEM server started successfully" << std::endl;
    std::cout << "[TEST] Waiting for log forwarder to connect..." << std::endl;
    std::cout << "\n>> Now run: ./log_forwarder\n" << std::endl;

    // Accept connection
    int clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == -1) {
        std::cerr << "[TEST] accept failed: " << strerror(errno) << std::endl;
        close(listenSocket);
        return 1;
    }

    // Handle client and run tests
    handleClient(clientSocket);

    // Cleanup
    close(listenSocket);

    std::cout << "\n[TEST] Test server shutting down..." << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
