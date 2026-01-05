#ifndef EVENT_LOG_READER_H
#define EVENT_LOG_READER_H

#include <string>
#include <vector>
#include <memory>

// Windows headers must be included in specific order
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winevt.h>

// Link with wevtapi.lib
#pragma comment(lib, "wevtapi.lib")

struct EventData {
    std::string timestamp;
    std::string eventId;
    std::string level;
    std::string source;
    std::string message;
    std::string computer;
};

class EventLogReader {
public:
    EventLogReader(const std::string& channel = "Application");
    ~EventLogReader();

    bool initialize();
    std::vector<EventData> readEvents(int maxEvents = 100);
    void close();

private:
    std::string channelName;
    EVT_HANDLE hSubscription;
    bool initialized;

    std::string getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId);
    std::string renderEventXml(EVT_HANDLE hEvent);
};

#endif // EVENT_LOG_READER_H
