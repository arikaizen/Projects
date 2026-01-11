# Network Packet Capture Module

This module provides network packet capture functionality similar to Wireshark, using the Npcap/WinPcap library.

## Overview

The `network_packet_reader` module allows you to:
- Capture live network packets from any interface
- Parse Ethernet, IP, TCP, and UDP headers
- Extract packet metadata (MAC addresses, IP addresses, ports, flags)
- Format packets as JSON (for SIEM), plain text, or hex dump
- Apply BPF filters (e.g., "tcp port 80", "host 192.168.1.1")

## Architecture

Similar to `event_log_reader`, this module provides:
- **Header file**: `inc/network_packet_reader.h` - API definitions
- **Implementation**: `src/network_packet_reader.cpp` - Packet capture and parsing
- **Multiple output formats**: JSON, plain text, hex dump

## Requirements

### 1. Npcap Driver (Runtime)
- **Download**: https://npcap.com/#download
- **Purpose**: Provides low-level packet capture on Windows
- **Installation**: Run the Npcap installer (requires admin privileges)
- **Note**: This is what Wireshark uses on Windows

### 2. Npcap SDK (Compile-time)
- **Download**: https://npcap.com/#download (scroll to SDK section)
- **Purpose**: Provides headers and libraries for development
- **Installation**: Extract to a known location (e.g., `C:\npcap-sdk`)

### 3. Administrator Privileges
- Packet capture requires administrator/elevated privileges
- Run your program as Administrator

## Installation Steps

### Step 1: Install Npcap Driver
```batch
1. Download Npcap installer from https://npcap.com
2. Run installer as Administrator
3. During installation:
   - Check "WinPcap API-compatible Mode" (for compatibility)
   - Check "Install Npcap in WinPcap API-compatible Mode"
4. Restart computer
```

### Step 2: Install Npcap SDK
```batch
1. Download Npcap SDK from https://npcap.com
2. Extract to C:\npcap-sdk
3. Folder structure should be:
   C:\npcap-sdk\
     Include\
       pcap.h
       ...
     Lib\
       x64\
         wpcap.lib
         Packet.lib
```

### Step 3: Update Build Configuration

Add to your compiler include path:
```
-IC:\npcap-sdk\Include
```

Add to your linker library path:
```
-LC:\npcap-sdk\Lib\x64
```

Link with:
```
-lwpcap -lPacket
```

## API Usage Examples

### Example 1: List Network Interfaces

```cpp
#include "network_packet_reader.h"
#include <iostream>

int main() {
    auto interfaces = getNetworkInterfaces();

    std::cout << "Available network interfaces:" << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << "Name: " << iface.name << std::endl;
        std::cout << "Description: " << iface.description << std::endl;
        std::cout << "IP: " << iface.ipAddress << std::endl;
        std::cout << "Loopback: " << (iface.isLoopback ? "Yes" : "No") << std::endl;
        std::cout << "---" << std::endl;
    }

    return 0;
}
```

### Example 2: Capture Packets and Print as JSON

```cpp
#include "network_packet_reader.h"
#include <iostream>

int main() {
    // Configure capture
    PacketCaptureConfig config;
    config.interfaceName = "\\Device\\NPF_{GUID}";  // Use actual interface
    config.mode = PacketCaptureMode::PROMISCUOUS;
    config.filter = "tcp port 80";  // Capture only HTTP traffic
    config.maxPackets = 100;  // Capture 100 packets

    // Open interface
    char errorBuffer[256];
    pcap_t* handle = openCaptureInterface(config, errorBuffer);

    if (!handle) {
        std::cerr << "Error: " << errorBuffer << std::endl;
        return 1;
    }

    std::cout << "Capturing packets... (Ctrl+C to stop)" << std::endl;

    // Capture loop
    int packetCount = 0;
    while (config.maxPackets == 0 || packetCount < config.maxPackets) {
        const pcap_pkthdr* header;
        const unsigned char* data;

        if (capturePacket(handle, &header, &data)) {
            PacketInfo packet = parsePacket(header, data);
            std::string json = formatPacketAsJson(packet);

            std::cout << json << std::endl;
            packetCount++;
        }
    }

    closeCaptureInterface(handle);
    return 0;
}
```

### Example 3: Capture Packets and Print as Plain Text

```cpp
#include "network_packet_reader.h"
#include <iostream>

int main() {
    PacketCaptureConfig config;
    config.interfaceName = "\\Device\\NPF_{GUID}";
    config.filter = "icmp";  // Capture only ICMP (ping)

    char errorBuffer[256];
    pcap_t* handle = openCaptureInterface(config, errorBuffer);

    if (!handle) {
        std::cerr << "Error: " << errorBuffer << std::endl;
        return 1;
    }

    while (true) {
        const pcap_pkthdr* header;
        const unsigned char* data;

        if (capturePacket(handle, &header, &data)) {
            PacketInfo packet = parsePacket(header, data);
            std::string text = formatPacketAsPlainText(packet);

            std::cout << text << std::endl << std::endl;
        }
    }

    closeCaptureInterface(handle);
    return 0;
}
```

### Example 4: Hex Dump

```cpp
#include "network_packet_reader.h"
#include <iostream>

int main() {
    PacketCaptureConfig config;
    config.interfaceName = "\\Device\\NPF_{GUID}";

    char errorBuffer[256];
    pcap_t* handle = openCaptureInterface(config, errorBuffer);

    if (!handle) {
        std::cerr << "Error: " << errorBuffer << std::endl;
        return 1;
    }

    const pcap_pkthdr* header;
    const unsigned char* data;

    if (capturePacket(handle, &header, &data)) {
        std::string hexDump = formatPacketAsHexDump(data, header->caplen);
        std::cout << hexDump << std::endl;
    }

    closeCaptureInterface(handle);
    return 0;
}
```

## BPF Filter Examples

Common BPF (Berkeley Packet Filter) expressions:

```
tcp port 80                    # HTTP traffic
tcp port 443                   # HTTPS traffic
udp port 53                    # DNS traffic
host 192.168.1.1               # All traffic to/from this IP
src 192.168.1.1                # Traffic from this IP
dst 192.168.1.1                # Traffic to this IP
tcp and port 22                # SSH traffic
icmp                           # ICMP (ping) traffic
not port 22                    # Everything except SSH
tcp[tcpflags] & tcp-syn != 0   # TCP SYN packets
```

## Packet Information Structure

The `PacketInfo` structure contains:

- **Timestamp**: Capture time (seconds + microseconds)
- **Sizes**: Captured length and wire length
- **Ethernet Layer**: Source/destination MAC, EtherType
- **IP Layer**: Source/destination IP, protocol, TTL
- **Transport Layer**: Source/destination ports
- **TCP Flags**: SYN, ACK, FIN, RST, PSH
- **Payload**: Length and hex preview (first 64 bytes)
- **Protocol**: Identified application protocol

## Output Formats

### JSON Format
```json
{
  "timestamp": 1234567890,
  "microseconds": 123456,
  "capture_length": 74,
  "wire_length": 74,
  "src_mac": "00:11:22:33:44:55",
  "dst_mac": "aa:bb:cc:dd:ee:ff",
  "ether_type": 2048,
  "src_ip": "192.168.1.100",
  "dst_ip": "8.8.8.8",
  "ip_version": 4,
  "protocol": 6,
  "protocol_name": "TCP",
  "ttl": 64,
  "src_port": 54321,
  "dst_port": 80,
  "tcp_syn": true,
  "tcp_ack": false,
  "payload_length": 0
}
```

### Plain Text Format
```
========================================
Timestamp:       1234567890.123456
Length:          74 bytes (wire: 74)
Source MAC:      00:11:22:33:44:55
Dest MAC:        aa:bb:cc:dd:ee:ff
EtherType:       0x800
Source IP:       192.168.1.100:54321
Dest IP:         8.8.8.8:80
Protocol:        TCP (6)
TTL:             64
TCP Flags:       SYN
========================================
```

### Hex Dump Format
```
0000  aa bb cc dd ee ff 00 11 22 33 44 55 08 00 45 00  ........"3DU..E.
0010  00 3c ab cd 40 00 40 06 e1 2f c0 a8 01 64 08 08  .<..@.@../...d..
0020  08 08 d4 31 00 50 12 34 56 78 00 00 00 00 a0 02  ...1.P.4Vx......
0030  72 10 12 34 00 00 02 04 05 b4 04 02 08 0a        r..4..........
```

## Security Considerations

1. **Administrator Privileges**: Packet capture requires elevated privileges
2. **Sensitive Data**: Packets may contain passwords, keys, personal information
3. **Network Load**: Promiscuous mode captures all network traffic
4. **Legal**: Ensure you have authorization to capture network traffic

## Limitations

1. **Windows Only**: Uses Npcap which is Windows-specific
2. **IPv4 Focus**: Current implementation focuses on IPv4 (IPv6 can be added)
3. **Basic Protocol Parsing**: Supports Ethernet, IPv4, TCP, UDP, ICMP
4. **No Reassembly**: Does not reassemble fragmented IP packets or TCP streams

## Troubleshooting

### "Npcap SDK required for packet capture"
- Install Npcap SDK and update build configuration
- Uncomment `#pragma comment(lib, "wpcap.lib")` in source

### "Access Denied" or "Operation not permitted"
- Run program as Administrator
- Check that Npcap driver is installed

### "No interfaces found"
- Install Npcap driver
- Restart computer after installation
- Check Windows Services for "Npcap Packet Driver"

### Compilation errors about missing pcap.h
- Install Npcap SDK
- Add SDK Include directory to compiler include path

## Integration with SIEM Forwarder

This module integrates seamlessly with the event log forwarder:

```cpp
// Capture network packets
PacketInfo packet = parsePacket(header, data);

// Format as JSON
std::string json = formatPacketAsJson(packet);

// Forward to SIEM (same as event logs)
sendToSIEM(json);
```

## Next Steps

To complete the implementation:
1. Install Npcap and Npcap SDK
2. Update build scripts to include Npcap SDK paths
3. Create test programs
4. Integrate with main forwarder application

## References

- Npcap: https://npcap.com
- WinPcap Documentation: https://www.winpcap.org/docs/
- BPF Filter Syntax: https://biot.com/capstats/bpf.html
- Wireshark: https://www.wireshark.org
