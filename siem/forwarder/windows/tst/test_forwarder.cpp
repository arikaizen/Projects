#include <iostream>
#include <cassert>
#include "../inc/event_log_reader.h"
#include "../inc/log_forwarder.h"

void testEventLogReader() {
    std::cout << "Testing EventLogReader..." << std::endl;

    EventLogReader reader("Application");
    assert(reader.initialize() && "Failed to initialize EventLogReader");

    std::vector<EventData> events = reader.readEvents(5);
    std::cout << "Read " << events.size() << " events" << std::endl;

    for (const auto& event : events) {
        std::cout << "Event ID: " << event.eventId
                  << ", Source: " << event.source
                  << ", Level: " << event.level << std::endl;
    }

    reader.close();
    std::cout << "EventLogReader test passed!\n" << std::endl;
}

void testLogForwarder() {
    std::cout << "Testing LogForwarder..." << std::endl;

    LogForwarder forwarder("127.0.0.1", 5000);
    assert(forwarder.initialize() && "Failed to initialize LogForwarder");

    std::cout << "Note: Connection test requires SIEM server running on 127.0.0.1:5000" << std::endl;
    std::cout << "Attempting connection..." << std::endl;

    if (forwarder.connect()) {
        std::cout << "Connection successful!" << std::endl;

        // Create test event
        EventData testEvent;
        testEvent.timestamp = "2024-01-01T12:00:00";
        testEvent.eventId = "1000";
        testEvent.level = "Information";
        testEvent.source = "TestSource";
        testEvent.computer = "TestComputer";
        testEvent.message = "Test event message";

        if (forwarder.sendEvent(testEvent)) {
            std::cout << "Test event sent successfully!" << std::endl;
        } else {
            std::cout << "Failed to send test event" << std::endl;
        }

        forwarder.disconnect();
    } else {
        std::cout << "Connection failed (server may not be running)" << std::endl;
    }

    std::cout << "LogForwarder test completed!\n" << std::endl;
}

int main() {
    std::cout << "======================================\n";
    std::cout << "Windows Event Log Forwarder Test Suite\n";
    std::cout << "======================================\n\n";

    try {
        testEventLogReader();
        testLogForwarder();

        std::cout << "======================================\n";
        std::cout << "All tests completed!\n";
        std::cout << "======================================\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
