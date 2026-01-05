#include "event_log_reader.h"
#include <iostream>
#include <sstream>
#include <iomanip>

EventLogReader::EventLogReader(const std::string& channel)
    : channelName(channel), hSubscription(NULL), initialized(false) {
}

EventLogReader::~EventLogReader() {
    close();
}

bool EventLogReader::initialize() {
    if (initialized) {
        return true;
    }

    // Open the event log channel
    std::wstring wChannelName(channelName.begin(), channelName.end());

    initialized = true;
    return true;
}

std::vector<EventData> EventLogReader::readEvents(int maxEvents) {
    std::vector<EventData> events;

    if (!initialized) {
        std::cerr << "EventLogReader not initialized" << std::endl;
        return events;
    }

    std::wstring wChannelName(channelName.begin(), channelName.end());

    // Query for events
    EVT_HANDLE hResults = EvtQuery(
        NULL,
        wChannelName.c_str(),
        NULL,
        EvtQueryChannelPath | EvtQueryReverseDirection
    );

    if (hResults == NULL) {
        std::cerr << "EvtQuery failed with error: " << GetLastError() << std::endl;
        return events;
    }

    DWORD dwReturned = 0;
    EVT_HANDLE hEvents[10];

    while (events.size() < maxEvents) {
        if (!EvtNext(hResults, 10, hEvents, INFINITE, 0, &dwReturned)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) {
                std::cerr << "EvtNext failed: " << GetLastError() << std::endl;
            }
            break;
        }

        for (DWORD i = 0; i < dwReturned && events.size() < maxEvents; i++) {
            EventData event;

            // Get event properties
            event.eventId = getEventProperty(hEvents[i], EvtSystemEventID);
            event.level = getEventProperty(hEvents[i], EvtSystemLevel);
            event.source = getEventProperty(hEvents[i], EvtSystemProviderName);
            event.computer = getEventProperty(hEvents[i], EvtSystemComputer);
            event.timestamp = getEventProperty(hEvents[i], EvtSystemTimeCreated);
            event.message = renderEventXml(hEvents[i]);

            events.push_back(event);
            EvtClose(hEvents[i]);
        }
    }

    EvtClose(hResults);
    return events;
}

std::string EventLogReader::getEventProperty(EVT_HANDLE hEvent, EVT_SYSTEM_PROPERTY_ID propertyId) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;

    // Get required buffer size
    EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);

    if (dwBufferUsed == 0) {
        return "";
    }

    std::vector<BYTE> buffer(dwBufferUsed);

    if (!EvtRender(NULL, hEvent, EvtRenderEventValues, dwBufferUsed, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
        return "";
    }

    PEVT_VARIANT pRenderedValues = (PEVT_VARIANT)buffer.data();

    if (propertyId < dwPropertyCount) {
        EVT_VARIANT& value = pRenderedValues[propertyId];

        std::ostringstream oss;
        switch (value.Type) {
            case EvtVarTypeString:
                if (value.StringVal) {
                    std::wstring ws(value.StringVal);
                    return std::string(ws.begin(), ws.end());
                }
                break;
            case EvtVarTypeUInt16:
                oss << value.UInt16Val;
                return oss.str();
            case EvtVarTypeUInt32:
                oss << value.UInt32Val;
                return oss.str();
            case EvtVarTypeFileTime:
                oss << value.FileTimeVal;
                return oss.str();
        }
    }

    return "";
}

std::string EventLogReader::renderEventXml(EVT_HANDLE hEvent) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;

    EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);

    if (dwBufferUsed == 0) {
        return "";
    }

    std::vector<WCHAR> buffer(dwBufferUsed / sizeof(WCHAR));

    if (EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferUsed, buffer.data(), &dwBufferUsed, &dwPropertyCount)) {
        std::wstring ws(buffer.data());
        return std::string(ws.begin(), ws.end());
    }

    return "";
}

void EventLogReader::close() {
    if (hSubscription != NULL) {
        EvtClose(hSubscription);
        hSubscription = NULL;
    }
    initialized = false;
}
