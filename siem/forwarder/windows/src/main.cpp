/*
 * Windows Event Log Forwarder for SIEM
 * Reads Windows Event Logs and forwards them to SIEM server on port 8089
 */

#include <windows.h>
#include <winevt.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <thread>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ws2_32.lib")

const char* SIEM_SERVER = "127.0.0.1";
const int SIEM_PORT = 8089;
const int BUFFER_SIZE = 65536;
const int RECONNECT_DELAY_MS = 5000;

class WindowsEventLogForwarder {
private:
    SOCKET sock;
    bool connected;
    std::string serverAddress;
    int serverPort;

public:
    WindowsEventLogForwarder(const std::string& server, int port)
        : serverAddress(server), serverPort(port), connected(false), sock(INVALID_SOCKET) {
    }

    ~WindowsEventLogForwarder() {
        disconnect();
    }

    bool initialize() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }
        return true;
    }

    bool connect() {
        struct addrinfo *result = NULL, *ptr = NULL, hints;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        std::string portStr = std::to_string(serverPort);
        int iResult = getaddrinfo(serverAddress.c_str(), portStr.c_str(), &hints, &result);
        if (iResult != 0) {
            std::cerr << "getaddrinfo failed: " << iResult << std::endl;
            return false;
        }

        sock = INVALID_SOCKET;
        for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (sock == INVALID_SOCKET) {
                std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
                continue;
            }

            iResult = ::connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen);
            if (iResult == SOCKET_ERROR) {
                closesocket(sock);
                sock = INVALID_SOCKET;
                continue;
            }
            break;
        }

        freeaddrinfo(result);

        if (sock == INVALID_SOCKET) {
            std::cerr << "Unable to connect to server!" << std::endl;
            return false;
        }

        connected = true;
        std::cout << "Connected to SIEM server at " << serverAddress << ":" << serverPort << std::endl;
        return true;
    }

    void disconnect() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        connected = false;
        WSACleanup();
    }

    bool sendLog(const std::string& logData) {
        if (!connected || sock == INVALID_SOCKET) {
            std::cerr << "Not connected to server" << std::endl;
            return false;
        }

        std::string message = logData + "\n";
        int result = send(sock, message.c_str(), (int)message.length(), 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "send failed: " << WSAGetLastError() << std::endl;
            connected = false;
            return false;
        }

        return true;
    }

    bool isConnected() const {
        return connected;
    }
};

std::string escapeJson(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;
    std::string result = "";

    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);
            if (pRenderedValues) {
                EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount);
            }
        }
    }

    if (pRenderedValues && propertyId < dwPropertyCount) {
        PEVT_VARIANT pProperty = &pRenderedValues[propertyId];
        if (pProperty->Type == EvtVarTypeString && pProperty->StringVal) {
            int size = WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, NULL, 0, NULL, NULL);
            if (size > 0) {
                char* buffer = new char[size];
                WideCharToMultiByte(CP_UTF8, 0, pProperty->StringVal, -1, buffer, size, NULL, NULL);
                result = buffer;
                delete[] buffer;
            }
        } else if (pProperty->Type == EvtVarTypeUInt16) {
            result = std::to_string(pProperty->UInt16Val);
        } else if (pProperty->Type == EvtVarTypeUInt32) {
            result = std::to_string(pProperty->UInt32Val);
        } else if (pProperty->Type == EvtVarTypeUInt64) {
            result = std::to_string(pProperty->UInt64Val);
        }
    }

    if (pRenderedValues) {
        free(pRenderedValues);
    }

    return result;
}

std::string formatEventAsJson(EVT_HANDLE hEvent) {
    std::ostringstream json;

    // Get event properties
    std::string eventId = getEventProperty(hEvent, EvtSystemEventID);
    std::string level = getEventProperty(hEvent, EvtSystemLevel);
    std::string channel = getEventProperty(hEvent, EvtSystemChannel);
    std::string computer = getEventProperty(hEvent, EvtSystemComputer);

    // Get timestamp
    ULONGLONG timestamp = 0;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;

    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);
            if (pRenderedValues) {
                EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount);
                if (dwPropertyCount > EvtSystemTimeCreated) {
                    timestamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
                }
                free(pRenderedValues);
            }
        }
    }

    // Build JSON
    json << "{";
    json << "\"event_id\":\"" << escapeJson(eventId) << "\",";
    json << "\"level\":\"" << escapeJson(level) << "\",";
    json << "\"channel\":\"" << escapeJson(channel) << "\",";
    json << "\"computer\":\"" << escapeJson(computer) << "\",";
    json << "\"timestamp\":" << timestamp;
    json << "}";

    return json.str();
}

void forwardWindowsLogs(WindowsEventLogForwarder& forwarder, const std::wstring& channelPath) {
    EVT_HANDLE hSubscription = NULL;
    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;

    // Subscribe to the channel
    hSubscription = EvtSubscribe(
        NULL,
        NULL,
        channelPath.c_str(),
        L"*",
        NULL,
        NULL,
        NULL,
        EvtSubscribeToFutureEvents
    );

    if (hSubscription == NULL) {
        std::cerr << "Failed to subscribe to event log. Error: " << GetLastError() << std::endl;
        return;
    }

    std::cout << "Monitoring Windows Event Logs..." << std::endl;

    while (true) {
        if (EvtNext(hSubscription, 10, hEvents, INFINITE, 0, &dwReturned)) {
            for (DWORD i = 0; i < dwReturned; i++) {
                std::string jsonLog = formatEventAsJson(hEvents[i]);

                if (!forwarder.isConnected()) {
                    std::cout << "Attempting to reconnect..." << std::endl;
                    if (!forwarder.connect()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                        continue;
                    }
                }

                if (forwarder.sendLog(jsonLog)) {
                    std::cout << "Forwarded: " << jsonLog << std::endl;
                } else {
                    std::cerr << "Failed to forward log" << std::endl;
                }

                EvtClose(hEvents[i]);
            }
        } else {
            DWORD status = GetLastError();
            if (status != ERROR_NO_MORE_ITEMS) {
                std::cerr << "EvtNext failed: " << status << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (hSubscription) {
        EvtClose(hSubscription);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Windows Event Log Forwarder for SIEM" << std::endl;
    std::cout << "=====================================" << std::endl;

    std::string server = SIEM_SERVER;
    int port = SIEM_PORT;

    // Parse command line arguments
    if (argc >= 2) {
        server = argv[1];
    }
    if (argc >= 3) {
        port = std::atoi(argv[2]);
    }

    std::cout << "Server: " << server << ":" << port << std::endl;

    WindowsEventLogForwarder forwarder(server, port);

    if (!forwarder.initialize()) {
        std::cerr << "Failed to initialize forwarder" << std::endl;
        return 1;
    }

    // Try to connect
    while (!forwarder.connect()) {
        std::cout << "Waiting to connect to SIEM server..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
    }

    // Monitor common Windows Event Log channels
    std::vector<std::wstring> channels = {
        L"System",
        L"Application",
        L"Security"
    };

    // For simplicity, we'll monitor the System channel
    // In production, you'd want to spawn threads for each channel
    forwardWindowsLogs(forwarder, L"System");

    return 0;
}
