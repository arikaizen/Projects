/**
 * @file network_packet_reader.h
 * @brief Network packet capture and analysis API (similar to Wireshark)
 *
 * This module provides functionality to capture network packets using WinPcap/Npcap,
 * parse protocol headers, and format packet data for SIEM forwarding.
 */

#ifndef NETWORK_PACKET_READER_H
#define NETWORK_PACKET_READER_H

#include <string>
#include <vector>
#include <windows.h>

// Forward declarations for pcap structures
struct pcap;
typedef struct pcap pcap_t;
struct pcap_pkthdr;

/**
 * @enum PacketCaptureMode
 * @brief Defines how packets should be captured
 */
enum class PacketCaptureMode {
    REALTIME,          ///< Capture packets in real-time
    PROMISCUOUS,       ///< Capture all packets on network (promiscuous mode)
    NON_PROMISCUOUS    ///< Capture only packets destined for this machine
};

/**
 * @enum PacketProtocol
 * @brief Common network protocols
 */
enum class PacketProtocol {
    UNKNOWN = 0,
    ICMP = 1,
    TCP = 6,
    UDP = 17,
    HTTP = 80,
    HTTPS = 443,
    DNS = 53,
    SSH = 22,
    FTP = 21,
    SMTP = 25
};

/**
 * @struct NetworkInterface
 * @brief Represents a network interface available for capture
 */
struct NetworkInterface {
    std::string name;           ///< Interface name (e.g., "\Device\NPF_{GUID}")
    std::string description;    ///< Human-readable description
    std::string ipAddress;      ///< IP address assigned to interface
    bool isLoopback;           ///< True if this is loopback interface
};

/**
 * @struct PacketInfo
 * @brief Parsed packet information
 */
struct PacketInfo {
    // Timestamp
    unsigned long timestamp;        ///< Capture timestamp (seconds since epoch)
    unsigned long microseconds;     ///< Microsecond component of timestamp

    // Sizes
    unsigned int captureLength;     ///< Number of bytes captured
    unsigned int wireLength;        ///< Actual packet size on wire

    // Ethernet layer
    std::string srcMAC;            ///< Source MAC address
    std::string dstMAC;            ///< Destination MAC address
    unsigned short etherType;      ///< EtherType (0x0800 = IPv4, 0x86DD = IPv6)

    // IP layer
    std::string srcIP;             ///< Source IP address
    std::string dstIP;             ///< Destination IP address
    unsigned char ipVersion;       ///< IP version (4 or 6)
    unsigned char protocol;        ///< Protocol (TCP=6, UDP=17, ICMP=1)
    unsigned char ttl;             ///< Time to live

    // Transport layer
    unsigned short srcPort;        ///< Source port (TCP/UDP)
    unsigned short dstPort;        ///< Destination port (TCP/UDP)

    // TCP flags
    bool tcpSYN;                   ///< TCP SYN flag
    bool tcpACK;                   ///< TCP ACK flag
    bool tcpFIN;                   ///< TCP FIN flag
    bool tcpRST;                   ///< TCP RST flag
    bool tcpPSH;                   ///< TCP PSH flag

    // Payload
    std::string payloadPreview;    ///< First 64 bytes of payload (hex)
    unsigned int payloadLength;    ///< Total payload length

    // Protocol identification
    PacketProtocol identifiedProtocol; ///< Identified application protocol
};

/**
 * @struct PacketCaptureConfig
 * @brief Configuration for packet capture
 */
struct PacketCaptureConfig {
    std::string interfaceName;     ///< Interface to capture on
    PacketCaptureMode mode;        ///< Capture mode
    int snapLength;                ///< Snapshot length (bytes to capture per packet)
    int timeout;                   ///< Read timeout in milliseconds
    std::string filter;            ///< BPF filter expression (e.g., "tcp port 80")
    int maxPackets;                ///< Maximum packets to capture (0 = unlimited)

    /**
     * @brief Constructor with default values
     */
    PacketCaptureConfig()
        : interfaceName(""),
          mode(PacketCaptureMode::NON_PROMISCUOUS),
          snapLength(65535),  // Maximum packet size
          timeout(1000),      // 1 second timeout
          filter(""),
          maxPackets(0) {}
};

/**
 * @brief Get list of available network interfaces
 *
 * Queries the system for all network interfaces that can be used
 * for packet capture.
 *
 * @return Vector of NetworkInterface structures
 */
std::vector<NetworkInterface> getNetworkInterfaces();

/**
 * @brief Open a network interface for packet capture
 *
 * Opens the specified network interface and prepares it for packet capture.
 * Requires administrator/elevated privileges.
 *
 * @param config Capture configuration
 * @param errorBuffer Buffer to receive error messages (at least 256 bytes)
 * @return pcap_t handle on success, NULL on failure
 */
pcap_t* openCaptureInterface(const PacketCaptureConfig& config, char* errorBuffer);

/**
 * @brief Capture a single packet
 *
 * Captures one packet from the network interface.
 *
 * @param handle pcap_t handle from openCaptureInterface
 * @param header Output parameter for packet header
 * @param data Output parameter for packet data
 * @return True if packet captured, false on timeout or error
 */
bool capturePacket(pcap_t* handle, const pcap_pkthdr** header, const unsigned char** data);

/**
 * @brief Parse packet data into PacketInfo structure
 *
 * Parses raw packet bytes and extracts protocol information.
 *
 * @param header Packet header from capturePacket
 * @param data Packet data from capturePacket
 * @return PacketInfo structure with parsed data
 */
PacketInfo parsePacket(const pcap_pkthdr* header, const unsigned char* data);

/**
 * @brief Format packet as JSON
 *
 * Formats packet information as JSON for SIEM forwarding.
 *
 * @param packet Parsed packet information
 * @return JSON-formatted string
 */
std::string formatPacketAsJson(const PacketInfo& packet);

/**
 * @brief Format packet as plain text
 *
 * Formats packet information as human-readable plain text.
 *
 * @param packet Parsed packet information
 * @return Plain text formatted string
 */
std::string formatPacketAsPlainText(const PacketInfo& packet);

/**
 * @brief Format packet as PCAP hex dump
 *
 * Formats packet data as hexadecimal dump (similar to tcpdump -X).
 *
 * @param data Raw packet data
 * @param length Length of packet data
 * @return Hex dump string
 */
std::string formatPacketAsHexDump(const unsigned char* data, unsigned int length);

/**
 * @brief Close packet capture handle
 *
 * Closes the packet capture handle and releases resources.
 *
 * @param handle pcap_t handle to close
 */
void closeCaptureInterface(pcap_t* handle);

/**
 * @brief Get protocol name from protocol number
 *
 * Converts IP protocol number to human-readable name.
 *
 * @param protocol Protocol number (e.g., 6 = TCP)
 * @return Protocol name string
 */
std::string getProtocolName(unsigned char protocol);

/**
 * @brief Check if running with administrator privileges
 *
 * Packet capture typically requires elevated privileges on Windows.
 *
 * @return True if running as administrator, false otherwise
 */
bool isAdministrator();

#endif // NETWORK_PACKET_READER_H
