#include <iostream>
#include <thread>
#include <chrono>
#include "event_log_reader.h"
#include "log_forwarder.h"

void printUsage() {
    std::cout << "Windows Event Log Forwarder for SIEM" << std::endl;
    std::cout << "Usage: log_forwarder <server> <port> [channel] [interval]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  server   - SIEM server address (e.g., 192.168.1.100)" << std::endl;
    std::cout << "  port     - SIEM server port (e.g., 5000)" << std::endl;
    std::cout << "  channel  - Event log channel (default: Application)" << std::endl;
    std::cout << "             Options: Application, Security, System, etc." << std::endl;
    std::cout << "  interval - Polling interval in seconds (default: 60)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  log_forwarder 192.168.1.100 5000 Security 30" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string serverAddress = argv[1];
    int serverPort = std::atoi(argv[2]);
    std::string channel = (argc > 3) ? argv[3] : "Application";
    int interval = (argc > 4) ? std::atoi(argv[4]) : 60;

    std::cout << "===========================================\n";
    std::cout << "Windows Event Log Forwarder\n";
    std::cout << "===========================================\n";
    std::cout << "Server: " << serverAddress << ":" << serverPort << std::endl;
    std::cout << "Channel: " << channel << std::endl;
    std::cout << "Interval: " << interval << " seconds" << std::endl;
    std::cout << "===========================================\n\n";

    // Initialize event log reader
    EventLogReader reader(channel);
    if (!reader.initialize()) {
        std::cerr << "Failed to initialize event log reader" << std::endl;
        return 1;
    }

    // Initialize log forwarder
    LogForwarder forwarder(serverAddress, serverPort);
    if (!forwarder.initialize()) {
        std::cerr << "Failed to initialize log forwarder" << std::endl;
        return 1;
    }

    // Connect to SIEM server
    if (!forwarder.connect()) {
        std::cerr << "Failed to connect to SIEM server" << std::endl;
        return 1;
    }

    std::cout << "Starting event forwarding... (Press Ctrl+C to stop)\n\n";

    // Main loop
    int cycleCount = 0;
    while (true) {
        cycleCount++;
        std::cout << "[Cycle " << cycleCount << "] Reading events..." << std::endl;

        // Read events
        std::vector<EventData> events = reader.readEvents(100);

        if (!events.empty()) {
            std::cout << "Found " << events.size() << " events, forwarding..." << std::endl;

            // Forward events to SIEM
            if (forwarder.sendEvents(events)) {
                std::cout << "Events forwarded successfully" << std::endl;
            } else {
                std::cerr << "Failed to forward some events" << std::endl;
            }
        } else {
            std::cout << "No new events found" << std::endl;
        }

        std::cout << "Waiting " << interval << " seconds...\n" << std::endl;

        // Sleep for specified interval
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }

    // Cleanup
    forwarder.disconnect();
    reader.close();

    return 0;
}
