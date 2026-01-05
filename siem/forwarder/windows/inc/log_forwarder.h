#ifndef LOG_FORWARDER_H
#define LOG_FORWARDER_H

#include <string>
#include <vector>
#include "event_log_reader.h"

// Windows socket headers - ORDER MATTERS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Winsock library
#pragma comment(lib, "ws2_32.lib")

class LogForwarder {
public:
    LogForwarder(const std::string& server, int port);
    ~LogForwarder();

    bool initialize();
    bool connect();
    bool sendEvent(const EventData& event);
    bool sendEvents(const std::vector<EventData>& events);
    void disconnect();

private:
    std::string serverAddress;
    int serverPort;
    SOCKET clientSocket;
    bool connected;
    bool wsaInitialized;

    std::string formatEventAsJson(const EventData& event);
};

#endif // LOG_FORWARDER_H
