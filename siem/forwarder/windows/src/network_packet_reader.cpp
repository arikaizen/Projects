/**
 * @file network_packet_reader.cpp
 * @brief Implementation of network packet capture and analysis
 *
 * Uses WinPcap/Npcap library for packet capture on Windows.
 * Requires Npcap SDK for compilation and Npcap driver for runtime.
 */

#include "network_packet_reader.h"
#include "json_utils.h"
#include <sstream>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>

// Npcap/WinPcap headers (requires Npcap SDK)
// Note: These would normally be from the Npcap SDK
// For now, we'll provide minimal type definitions

// Minimal pcap structure definitions
struct pcap_if {
    struct pcap_if *next;
    char *name;
    char *description;
    struct pcap_addr *addresses;
    unsigned int flags;
};

struct pcap_addr {
    struct pcap_addr *next;
    struct sockaddr *addr;
    struct sockaddr *netmask;
    struct sockaddr *broadaddr;
    struct sockaddr *dstaddr;
};

struct pcap_pkthdr {
    unsigned long tv_sec;   // Timestamp seconds
    unsigned long tv_usec;  // Timestamp microseconds
    unsigned int caplen;    // Length of captured portion
    unsigned int len;       // Length of packet on wire
};

// Ethernet header (14 bytes)
struct EthernetHeader {
    unsigned char dstMAC[6];    // Destination MAC address
    unsigned char srcMAC[6];    // Source MAC address
    unsigned short etherType;   // EtherType field
};

// IPv4 header (minimum 20 bytes)
struct IPv4Header {
    unsigned char versionIHL;       // Version (4 bits) + IHL (4 bits)
    unsigned char typeOfService;    // Type of service
    unsigned short totalLength;     // Total length
    unsigned short identification;  // Identification
    unsigned short flagsFragment;   // Flags (3 bits) + Fragment offset (13 bits)
    unsigned char ttl;              // Time to live
    unsigned char protocol;         // Protocol
    unsigned short checksum;        // Header checksum
    unsigned int srcIP;             // Source IP address
    unsigned int dstIP;             // Destination IP address
};

// TCP header (minimum 20 bytes)
struct TCPHeader {
    unsigned short srcPort;         // Source port
    unsigned short dstPort;         // Destination port
    unsigned int sequenceNum;       // Sequence number
    unsigned int ackNum;            // Acknowledgment number
    unsigned char dataOffset;       // Data offset (4 bits) + reserved (4 bits)
    unsigned char flags;            // TCP flags
    unsigned short window;          // Window size
    unsigned short checksum;        // Checksum
    unsigned short urgentPointer;   // Urgent pointer
};

// UDP header (8 bytes)
struct UDPHeader {
    unsigned short srcPort;         // Source port
    unsigned short dstPort;         // Destination port
    unsigned short length;          // Length
    unsigned short checksum;        // Checksum
};

// TCP flag definitions
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

// EtherType definitions
#define ETHERTYPE_IP   0x0800  // IPv4
#define ETHERTYPE_ARP  0x0806  // ARP
#define ETHERTYPE_IPV6 0x86DD  // IPv6

// Pragma comment to link with required libraries
#pragma comment(lib, "ws2_32.lib")
// #pragma comment(lib, "wpcap.lib")  // Uncomment when Npcap SDK is available

/**
 * @brief Convert MAC address to string format
 */
std::string macToString(const unsigned char* mac) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        if (i > 0) oss << ":";
        oss << std::setw(2) << static_cast<int>(mac[i]);
    }
    return oss.str();
}

/**
 * @brief Convert IP address to string format
 */
std::string ipToString(unsigned int ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN);
    return std::string(buffer);
}

std::vector<NetworkInterface> getNetworkInterfaces() {
    std::vector<NetworkInterface> interfaces;

    // Note: This requires pcap_findalldevs from Npcap SDK
    // Placeholder implementation - would use:
    // pcap_if_t *alldevs;
    // char errbuf[PCAP_ERRBUF_SIZE];
    // if (pcap_findalldevs(&alldevs, errbuf) == -1) {
    //     return interfaces;
    // }

    // For now, return a placeholder
    NetworkInterface placeholder;
    placeholder.name = "\\Device\\NPF_{ADAPTER-GUID}";
    placeholder.description = "Network Interface (requires Npcap)";
    placeholder.ipAddress = "0.0.0.0";
    placeholder.isLoopback = false;
    interfaces.push_back(placeholder);

    return interfaces;
}

pcap_t* openCaptureInterface(const PacketCaptureConfig& config, char* errorBuffer) {
    // Note: This requires pcap_open_live from Npcap SDK
    // Placeholder implementation - would use:
    // pcap_t* handle = pcap_open_live(
    //     config.interfaceName.c_str(),
    //     config.snapLength,
    //     (config.mode == PacketCaptureMode::PROMISCUOUS) ? 1 : 0,
    //     config.timeout,
    //     errorBuffer
    // );

    // if (!config.filter.empty() && handle) {
    //     struct bpf_program fp;
    //     if (pcap_compile(handle, &fp, config.filter.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
    //         pcap_close(handle);
    //         return nullptr;
    //     }
    //     if (pcap_setfilter(handle, &fp) == -1) {
    //         pcap_freecode(&fp);
    //         pcap_close(handle);
    //         return nullptr;
    //     }
    //     pcap_freecode(&fp);
    // }

    strcpy(errorBuffer, "Npcap SDK required for packet capture");
    return nullptr;
}

bool capturePacket(pcap_t* handle, const pcap_pkthdr** header, const unsigned char** data) {
    // Note: This requires pcap_next_ex from Npcap SDK
    // Placeholder implementation - would use:
    // int result = pcap_next_ex(handle, header, data);
    // return (result == 1);

    return false;
}

PacketInfo parsePacket(const pcap_pkthdr* header, const unsigned char* data) {
    PacketInfo info;

    // Initialize all fields
    info.timestamp = header->tv_sec;
    info.microseconds = header->tv_usec;
    info.captureLength = header->caplen;
    info.wireLength = header->len;
    info.srcMAC = "";
    info.dstMAC = "";
    info.etherType = 0;
    info.srcIP = "";
    info.dstIP = "";
    info.ipVersion = 0;
    info.protocol = 0;
    info.ttl = 0;
    info.srcPort = 0;
    info.dstPort = 0;
    info.tcpSYN = false;
    info.tcpACK = false;
    info.tcpFIN = false;
    info.tcpRST = false;
    info.tcpPSH = false;
    info.payloadPreview = "";
    info.payloadLength = 0;
    info.identifiedProtocol = PacketProtocol::UNKNOWN;

    if (header->caplen < sizeof(EthernetHeader)) {
        return info; // Packet too small
    }

    // Parse Ethernet header
    const EthernetHeader* ethHeader = reinterpret_cast<const EthernetHeader*>(data);
    info.srcMAC = macToString(ethHeader->srcMAC);
    info.dstMAC = macToString(ethHeader->dstMAC);
    info.etherType = ntohs(ethHeader->etherType);

    // Check if this is IPv4
    if (info.etherType != ETHERTYPE_IP) {
        return info; // Not IPv4, return what we have
    }

    // Parse IPv4 header
    const unsigned char* ipData = data + sizeof(EthernetHeader);
    if (header->caplen < sizeof(EthernetHeader) + sizeof(IPv4Header)) {
        return info; // Packet too small
    }

    const IPv4Header* ipHeader = reinterpret_cast<const IPv4Header*>(ipData);
    info.ipVersion = (ipHeader->versionIHL >> 4) & 0x0F;
    info.protocol = ipHeader->protocol;
    info.ttl = ipHeader->ttl;
    info.srcIP = ipToString(ipHeader->srcIP);
    info.dstIP = ipToString(ipHeader->dstIP);

    // Calculate IP header length
    unsigned char ihl = (ipHeader->versionIHL & 0x0F) * 4;
    const unsigned char* transportData = ipData + ihl;

    // Parse transport layer (TCP or UDP)
    if (info.protocol == 6) { // TCP
        if (header->caplen < sizeof(EthernetHeader) + ihl + sizeof(TCPHeader)) {
            return info; // Packet too small
        }

        const TCPHeader* tcpHeader = reinterpret_cast<const TCPHeader*>(transportData);
        info.srcPort = ntohs(tcpHeader->srcPort);
        info.dstPort = ntohs(tcpHeader->dstPort);

        // Parse TCP flags
        info.tcpSYN = (tcpHeader->flags & TCP_SYN) != 0;
        info.tcpACK = (tcpHeader->flags & TCP_ACK) != 0;
        info.tcpFIN = (tcpHeader->flags & TCP_FIN) != 0;
        info.tcpRST = (tcpHeader->flags & TCP_RST) != 0;
        info.tcpPSH = (tcpHeader->flags & TCP_PSH) != 0;

        // Calculate TCP header length
        unsigned char tcpHeaderLength = ((tcpHeader->dataOffset >> 4) & 0x0F) * 4;
        unsigned int totalHeaderLength = sizeof(EthernetHeader) + ihl + tcpHeaderLength;

        if (header->caplen > totalHeaderLength) {
            info.payloadLength = header->caplen - totalHeaderLength;
            const unsigned char* payload = data + totalHeaderLength;

            // Create hex preview of first 64 bytes
            std::ostringstream hexPreview;
            unsigned int previewLength = (info.payloadLength > 64) ? 64 : info.payloadLength;
            for (unsigned int i = 0; i < previewLength; i++) {
                hexPreview << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(payload[i]);
            }
            info.payloadPreview = hexPreview.str();
        }

        // Identify application protocol by port
        if (info.dstPort == 80 || info.srcPort == 80) {
            info.identifiedProtocol = PacketProtocol::HTTP;
        } else if (info.dstPort == 443 || info.srcPort == 443) {
            info.identifiedProtocol = PacketProtocol::HTTPS;
        } else if (info.dstPort == 22 || info.srcPort == 22) {
            info.identifiedProtocol = PacketProtocol::SSH;
        } else if (info.dstPort == 21 || info.srcPort == 21) {
            info.identifiedProtocol = PacketProtocol::FTP;
        } else if (info.dstPort == 25 || info.srcPort == 25) {
            info.identifiedProtocol = PacketProtocol::SMTP;
        } else {
            info.identifiedProtocol = PacketProtocol::TCP;
        }

    } else if (info.protocol == 17) { // UDP
        if (header->caplen < sizeof(EthernetHeader) + ihl + sizeof(UDPHeader)) {
            return info; // Packet too small
        }

        const UDPHeader* udpHeader = reinterpret_cast<const UDPHeader*>(transportData);
        info.srcPort = ntohs(udpHeader->srcPort);
        info.dstPort = ntohs(udpHeader->dstPort);

        unsigned int totalHeaderLength = sizeof(EthernetHeader) + ihl + sizeof(UDPHeader);

        if (header->caplen > totalHeaderLength) {
            info.payloadLength = header->caplen - totalHeaderLength;
            const unsigned char* payload = data + totalHeaderLength;

            // Create hex preview
            std::ostringstream hexPreview;
            unsigned int previewLength = (info.payloadLength > 64) ? 64 : info.payloadLength;
            for (unsigned int i = 0; i < previewLength; i++) {
                hexPreview << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(payload[i]);
            }
            info.payloadPreview = hexPreview.str();
        }

        // Identify application protocol by port
        if (info.dstPort == 53 || info.srcPort == 53) {
            info.identifiedProtocol = PacketProtocol::DNS;
        } else {
            info.identifiedProtocol = PacketProtocol::UDP;
        }

    } else if (info.protocol == 1) { // ICMP
        info.identifiedProtocol = PacketProtocol::ICMP;
    }

    return info;
}

std::string formatPacketAsJson(const PacketInfo& packet) {
    std::ostringstream json;

    json << "{";
    json << "\"timestamp\":" << packet.timestamp << ",";
    json << "\"microseconds\":" << packet.microseconds << ",";
    json << "\"capture_length\":" << packet.captureLength << ",";
    json << "\"wire_length\":" << packet.wireLength << ",";
    json << "\"src_mac\":\"" << escapeJson(packet.srcMAC) << "\",";
    json << "\"dst_mac\":\"" << escapeJson(packet.dstMAC) << "\",";
    json << "\"ether_type\":" << packet.etherType << ",";
    json << "\"src_ip\":\"" << escapeJson(packet.srcIP) << "\",";
    json << "\"dst_ip\":\"" << escapeJson(packet.dstIP) << "\",";
    json << "\"ip_version\":" << static_cast<int>(packet.ipVersion) << ",";
    json << "\"protocol\":" << static_cast<int>(packet.protocol) << ",";
    json << "\"protocol_name\":\"" << getProtocolName(packet.protocol) << "\",";
    json << "\"ttl\":" << static_cast<int>(packet.ttl) << ",";
    json << "\"src_port\":" << packet.srcPort << ",";
    json << "\"dst_port\":" << packet.dstPort << ",";
    json << "\"tcp_syn\":" << (packet.tcpSYN ? "true" : "false") << ",";
    json << "\"tcp_ack\":" << (packet.tcpACK ? "true" : "false") << ",";
    json << "\"tcp_fin\":" << (packet.tcpFIN ? "true" : "false") << ",";
    json << "\"tcp_rst\":" << (packet.tcpRST ? "true" : "false") << ",";
    json << "\"tcp_psh\":" << (packet.tcpPSH ? "true" : "false") << ",";
    json << "\"payload_length\":" << packet.payloadLength << ",";
    json << "\"payload_preview\":\"" << escapeJson(packet.payloadPreview) << "\"";
    json << "}";

    return json.str();
}

std::string formatPacketAsPlainText(const PacketInfo& packet) {
    std::ostringstream text;

    text << "========================================" << std::endl;
    text << "Timestamp:       " << packet.timestamp << "." << packet.microseconds << std::endl;
    text << "Length:          " << packet.captureLength << " bytes (wire: " << packet.wireLength << ")" << std::endl;
    text << "Source MAC:      " << packet.srcMAC << std::endl;
    text << "Dest MAC:        " << packet.dstMAC << std::endl;
    text << "EtherType:       0x" << std::hex << packet.etherType << std::dec << std::endl;

    if (!packet.srcIP.empty()) {
        text << "Source IP:       " << packet.srcIP;
        if (packet.srcPort > 0) text << ":" << packet.srcPort;
        text << std::endl;

        text << "Dest IP:         " << packet.dstIP;
        if (packet.dstPort > 0) text << ":" << packet.dstPort;
        text << std::endl;

        text << "Protocol:        " << getProtocolName(packet.protocol)
             << " (" << static_cast<int>(packet.protocol) << ")" << std::endl;
        text << "TTL:             " << static_cast<int>(packet.ttl) << std::endl;

        if (packet.protocol == 6) { // TCP
            text << "TCP Flags:       ";
            if (packet.tcpSYN) text << "SYN ";
            if (packet.tcpACK) text << "ACK ";
            if (packet.tcpFIN) text << "FIN ";
            if (packet.tcpRST) text << "RST ";
            if (packet.tcpPSH) text << "PSH ";
            text << std::endl;
        }

        if (packet.payloadLength > 0) {
            text << "Payload:         " << packet.payloadLength << " bytes" << std::endl;
            text << "Preview (hex):   " << packet.payloadPreview.substr(0, 32) << "..." << std::endl;
        }
    }

    text << "========================================";

    return text.str();
}

std::string formatPacketAsHexDump(const unsigned char* data, unsigned int length) {
    std::ostringstream dump;

    for (unsigned int i = 0; i < length; i += 16) {
        // Offset
        dump << std::setw(4) << std::setfill('0') << std::hex << i << "  ";

        // Hex bytes
        for (unsigned int j = 0; j < 16; j++) {
            if (i + j < length) {
                dump << std::setw(2) << std::setfill('0') << std::hex
                    << static_cast<int>(data[i + j]) << " ";
            } else {
                dump << "   ";
            }

            if (j == 7) dump << " "; // Extra space in middle
        }

        dump << " ";

        // ASCII representation
        for (unsigned int j = 0; j < 16 && i + j < length; j++) {
            unsigned char c = data[i + j];
            dump << (isprint(c) ? static_cast<char>(c) : '.');
        }

        dump << std::endl;
    }

    return dump.str();
}

void closeCaptureInterface(pcap_t* handle) {
    // Note: This requires pcap_close from Npcap SDK
    // Placeholder implementation - would use:
    // if (handle) {
    //     pcap_close(handle);
    // }
}

std::string getProtocolName(unsigned char protocol) {
    switch (protocol) {
        case 1:  return "ICMP";
        case 6:  return "TCP";
        case 17: return "UDP";
        case 2:  return "IGMP";
        case 41: return "IPv6";
        case 47: return "GRE";
        case 50: return "ESP";
        case 51: return "AH";
        case 58: return "ICMPv6";
        case 89: return "OSPF";
        case 132: return "SCTP";
        default: return "Unknown";
    }
}

bool isAdministrator() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin != FALSE;
}
