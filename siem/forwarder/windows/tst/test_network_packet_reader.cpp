/**
 * @file test_network_packet_reader.cpp
 * @brief Tests for Network Packet Reader (No Google Test dependency)
 *
 * Tests network packet capture, parsing, and formatting functions
 * Note: Some tests require Npcap SDK and administrator privileges
 */

#include <iostream>
#include <windows.h>
#include "network_packet_reader.h"
#include "logger.h"

// Windows Console Colors
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

int passed = 0;
int failed = 0;
int skipped = 0;

void enableConsoleColors() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, mode);
}

#define TEST_START(name) std::cout << COLOR_CYAN << "Testing: " << COLOR_RESET << name << "... "
#define TEST_PASS() { std::cout << COLOR_GREEN << "[PASS]" << COLOR_RESET << std::endl; passed++; }
#define TEST_FAIL(msg) { std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << " " << msg << std::endl; failed++; }
#define TEST_SKIP(msg) { std::cout << COLOR_YELLOW << "[SKIP]" << COLOR_RESET << " " << msg << std::endl; skipped++; }

#define ASSERT_TRUE(condition, msg) \
    if (!(condition)) { \
        std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << " " << msg << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Location: " << COLOR_RESET << __FILE__ << ":" << __LINE__ << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Condition: " << COLOR_RESET << #condition << std::endl; \
        failed++; \
        return false; \
    }

#define ASSERT_FALSE(condition, msg) \
    if (condition) { \
        std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << " " << msg << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Location: " << COLOR_RESET << __FILE__ << ":" << __LINE__ << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Condition: " << COLOR_RESET << "!(" << #condition << ")" << std::endl; \
        failed++; \
        return false; \
    }

#define ASSERT_EQUAL(actual, expected, msg) \
    if ((actual) != (expected)) { \
        std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << " " << msg << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Location: " << COLOR_RESET << __FILE__ << ":" << __LINE__ << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Expected: " << COLOR_RESET << (expected) << std::endl; \
        std::cout << "  " << COLOR_YELLOW << "Actual:   " << COLOR_RESET << (actual) << std::endl; \
        failed++; \
        return false; \
    }

// Helper: Create a mock Ethernet/IPv4/TCP packet
void createMockTcpPacket(unsigned char* buffer, unsigned int& length) {
    // Ethernet header (14 bytes)
    buffer[0] = 0xAA; buffer[1] = 0xBB; buffer[2] = 0xCC; // Dst MAC
    buffer[3] = 0xDD; buffer[4] = 0xEE; buffer[5] = 0xFF;
    buffer[6] = 0x11; buffer[7] = 0x22; buffer[8] = 0x33; // Src MAC
    buffer[9] = 0x44; buffer[10] = 0x55; buffer[11] = 0x66;
    buffer[12] = 0x08; buffer[13] = 0x00; // EtherType: IPv4

    // IPv4 header (20 bytes)
    buffer[14] = 0x45; // Version 4, IHL 5 (20 bytes)
    buffer[15] = 0x00; // TOS
    buffer[16] = 0x00; buffer[17] = 0x3C; // Total length: 60 bytes
    buffer[18] = 0x00; buffer[19] = 0x00; // Identification
    buffer[20] = 0x40; buffer[21] = 0x00; // Flags + Fragment offset
    buffer[22] = 0x40; // TTL: 64
    buffer[23] = 0x06; // Protocol: TCP
    buffer[24] = 0x00; buffer[25] = 0x00; // Checksum
    buffer[26] = 0xC0; buffer[27] = 0xA8; buffer[28] = 0x01; buffer[29] = 0x64; // Src IP: 192.168.1.100
    buffer[30] = 0x08; buffer[31] = 0x08; buffer[32] = 0x08; buffer[33] = 0x08; // Dst IP: 8.8.8.8

    // TCP header (20 bytes)
    buffer[34] = 0xD4; buffer[35] = 0x31; // Src port: 54321
    buffer[36] = 0x00; buffer[37] = 0x50; // Dst port: 80 (HTTP)
    buffer[38] = 0x12; buffer[39] = 0x34; buffer[40] = 0x56; buffer[41] = 0x78; // Sequence number
    buffer[42] = 0x00; buffer[43] = 0x00; buffer[44] = 0x00; buffer[45] = 0x00; // Ack number
    buffer[46] = 0x50; // Data offset: 5 (20 bytes)
    buffer[47] = 0x02; // Flags: SYN
    buffer[48] = 0x72; buffer[49] = 0x10; // Window size
    buffer[50] = 0x00; buffer[51] = 0x00; // Checksum
    buffer[52] = 0x00; buffer[53] = 0x00; // Urgent pointer

    length = 54; // Total: 14 + 20 + 20
}

// Helper: Create a mock pcap_pkthdr
void createMockPacketHeader(pcap_pkthdr* header, unsigned int packetLength) {
    header->tv_sec = 1234567890;
    header->tv_usec = 123456;
    header->caplen = packetLength;
    header->len = packetLength;
}

/**
 * Test: getNetworkInterfaces returns list
 */
bool test_GetNetworkInterfaces_ReturnsList() {
    TEST_START("getNetworkInterfaces - Returns list");

    auto interfaces = getNetworkInterfaces();

    // Should return at least a placeholder interface
    ASSERT_TRUE(interfaces.size() > 0, "No interfaces returned");

    // Check first interface has required fields
    ASSERT_FALSE(interfaces[0].name.empty(), "Interface name is empty");
    ASSERT_FALSE(interfaces[0].description.empty(), "Interface description is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: parsePacket with valid TCP packet
 */
bool test_ParsePacket_ValidTcpPacket() {
    TEST_START("parsePacket - Valid TCP packet");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);

    // Check timestamp
    ASSERT_EQUAL(packet.timestamp, 1234567890UL, "Incorrect timestamp");
    ASSERT_EQUAL(packet.microseconds, 123456UL, "Incorrect microseconds");

    // Check sizes
    ASSERT_EQUAL(packet.captureLength, length, "Incorrect capture length");
    ASSERT_EQUAL(packet.wireLength, length, "Incorrect wire length");

    // Check MAC addresses
    ASSERT_FALSE(packet.srcMAC.empty(), "Source MAC is empty");
    ASSERT_FALSE(packet.dstMAC.empty(), "Destination MAC is empty");
    ASSERT_TRUE(packet.srcMAC.find(":") != std::string::npos, "MAC format invalid");

    // Check IP addresses
    ASSERT_EQUAL(packet.srcIP, std::string("192.168.1.100"), "Incorrect source IP");
    ASSERT_EQUAL(packet.dstIP, std::string("8.8.8.8"), "Incorrect destination IP");

    // Check IP version and protocol
    ASSERT_EQUAL(static_cast<int>(packet.ipVersion), 4, "Incorrect IP version");
    ASSERT_EQUAL(static_cast<int>(packet.protocol), 6, "Incorrect protocol (should be TCP)");
    ASSERT_EQUAL(static_cast<int>(packet.ttl), 64, "Incorrect TTL");

    // Check ports
    ASSERT_EQUAL(packet.srcPort, 54321, "Incorrect source port");
    ASSERT_EQUAL(packet.dstPort, 80, "Incorrect destination port");

    // Check TCP flags
    ASSERT_TRUE(packet.tcpSYN, "TCP SYN flag not set");
    ASSERT_FALSE(packet.tcpACK, "TCP ACK flag incorrectly set");
    ASSERT_FALSE(packet.tcpFIN, "TCP FIN flag incorrectly set");

    // Check protocol identification
    ASSERT_TRUE(packet.identifiedProtocol == PacketProtocol::HTTP, "Protocol not identified as HTTP");

    TEST_PASS();
    return true;
}

/**
 * Test: parsePacket with UDP packet
 */
bool test_ParsePacket_ValidUdpPacket() {
    TEST_START("parsePacket - Valid UDP packet");

    unsigned char buffer[256];
    unsigned int length = 0;

    // Create UDP packet (similar to TCP but with UDP header)
    createMockTcpPacket(buffer, length); // Start with TCP packet structure
    buffer[23] = 0x11; // Change protocol to UDP (17)
    buffer[36] = 0x00; buffer[37] = 0x35; // Change dst port to 53 (DNS)

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);

    ASSERT_EQUAL(static_cast<int>(packet.protocol), 17, "Incorrect protocol (should be UDP)");
    ASSERT_EQUAL(packet.dstPort, 53, "Incorrect destination port");
    ASSERT_TRUE(packet.identifiedProtocol == PacketProtocol::DNS, "Protocol not identified as DNS");

    TEST_PASS();
    return true;
}

/**
 * Test: parsePacket with truncated packet
 */
bool test_ParsePacket_TruncatedPacket() {
    TEST_START("parsePacket - Truncated packet");

    unsigned char buffer[10]; // Only 10 bytes
    for (int i = 0; i < 10; i++) buffer[i] = 0x00;

    pcap_pkthdr header;
    header.tv_sec = 1234567890;
    header.tv_usec = 0;
    header.caplen = 10;
    header.len = 100; // Actual packet was 100 bytes

    PacketInfo packet = parsePacket(&header, buffer);

    // Should handle gracefully
    ASSERT_EQUAL(packet.captureLength, 10U, "Incorrect capture length");
    ASSERT_EQUAL(packet.wireLength, 100U, "Incorrect wire length");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsJson returns valid JSON
 */
bool test_FormatPacketAsJson_ReturnsValidJson() {
    TEST_START("formatPacketAsJson - Returns valid JSON");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string json = formatPacketAsJson(packet);

    // Check for JSON structure
    ASSERT_TRUE(json.find("{") != std::string::npos, "Missing opening brace");
    ASSERT_TRUE(json.find("}") != std::string::npos, "Missing closing brace");

    // Check for key fields
    ASSERT_TRUE(json.find("timestamp") != std::string::npos, "Missing timestamp field");
    ASSERT_TRUE(json.find("src_ip") != std::string::npos, "Missing src_ip field");
    ASSERT_TRUE(json.find("dst_ip") != std::string::npos, "Missing dst_ip field");
    ASSERT_TRUE(json.find("src_port") != std::string::npos, "Missing src_port field");
    ASSERT_TRUE(json.find("dst_port") != std::string::npos, "Missing dst_port field");
    ASSERT_TRUE(json.find("protocol") != std::string::npos, "Missing protocol field");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsJson includes IP addresses
 */
bool test_FormatPacketAsJson_IncludesIpAddresses() {
    TEST_START("formatPacketAsJson - Includes IP addresses");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string json = formatPacketAsJson(packet);

    // Check for specific IP addresses
    ASSERT_TRUE(json.find("192.168.1.100") != std::string::npos, "Missing source IP");
    ASSERT_TRUE(json.find("8.8.8.8") != std::string::npos, "Missing destination IP");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsJson consistent output
 */
bool test_FormatPacketAsJson_ConsistentOutput() {
    TEST_START("formatPacketAsJson - Consistent output");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string json1 = formatPacketAsJson(packet);
    std::string json2 = formatPacketAsJson(packet);

    ASSERT_TRUE(json1 == json2, "JSON output not consistent");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsPlainText returns formatted text
 */
bool test_FormatPacketAsPlainText_ReturnsText() {
    TEST_START("formatPacketAsPlainText - Returns text");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string text = formatPacketAsPlainText(packet);

    ASSERT_FALSE(text.empty(), "Plain text is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsPlainText contains standard fields
 */
bool test_FormatPacketAsPlainText_ContainsStandardFields() {
    TEST_START("formatPacketAsPlainText - Contains standard fields");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string text = formatPacketAsPlainText(packet);

    // Check for expected field labels
    ASSERT_TRUE(text.find("Timestamp") != std::string::npos, "Missing 'Timestamp' field");
    ASSERT_TRUE(text.find("Length") != std::string::npos, "Missing 'Length' field");
    ASSERT_TRUE(text.find("Source MAC") != std::string::npos, "Missing 'Source MAC' field");
    ASSERT_TRUE(text.find("Dest MAC") != std::string::npos, "Missing 'Dest MAC' field");
    ASSERT_TRUE(text.find("Source IP") != std::string::npos, "Missing 'Source IP' field");
    ASSERT_TRUE(text.find("Dest IP") != std::string::npos, "Missing 'Dest IP' field");
    ASSERT_TRUE(text.find("Protocol") != std::string::npos, "Missing 'Protocol' field");
    ASSERT_TRUE(text.find("TTL") != std::string::npos, "Missing 'TTL' field");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsPlainText includes separators
 */
bool test_FormatPacketAsPlainText_IncludesSeparators() {
    TEST_START("formatPacketAsPlainText - Includes separators");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string text = formatPacketAsPlainText(packet);

    ASSERT_TRUE(text.find("===") != std::string::npos, "Missing separator");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsPlainText shows TCP flags
 */
bool test_FormatPacketAsPlainText_ShowsTcpFlags() {
    TEST_START("formatPacketAsPlainText - Shows TCP flags");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string text = formatPacketAsPlainText(packet);

    ASSERT_TRUE(text.find("TCP Flags") != std::string::npos, "Missing 'TCP Flags' field");
    ASSERT_TRUE(text.find("SYN") != std::string::npos, "Missing SYN flag");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsHexDump returns hex output
 */
bool test_FormatPacketAsHexDump_ReturnsHex() {
    TEST_START("formatPacketAsHexDump - Returns hex");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    std::string hexDump = formatPacketAsHexDump(buffer, length);

    ASSERT_FALSE(hexDump.empty(), "Hex dump is empty");

    // Check for hex characters
    ASSERT_TRUE(hexDump.find("0000") != std::string::npos, "Missing offset");

    TEST_PASS();
    return true;
}

/**
 * Test: formatPacketAsHexDump correct format
 */
bool test_FormatPacketAsHexDump_CorrectFormat() {
    TEST_START("formatPacketAsHexDump - Correct format");

    unsigned char buffer[32];
    for (int i = 0; i < 32; i++) buffer[i] = i;

    std::string hexDump = formatPacketAsHexDump(buffer, 32);

    // Should have at least 2 lines (16 bytes per line)
    int lineCount = 0;
    size_t pos = 0;
    while ((pos = hexDump.find("\n", pos)) != std::string::npos) {
        lineCount++;
        pos++;
    }
    ASSERT_TRUE(lineCount >= 2, "Hex dump has too few lines");

    TEST_PASS();
    return true;
}

/**
 * Test: getProtocolName returns correct names
 */
bool test_GetProtocolName_ReturnsCorrectNames() {
    TEST_START("getProtocolName - Returns correct names");

    ASSERT_EQUAL(getProtocolName(1), std::string("ICMP"), "Incorrect protocol name for ICMP");
    ASSERT_EQUAL(getProtocolName(6), std::string("TCP"), "Incorrect protocol name for TCP");
    ASSERT_EQUAL(getProtocolName(17), std::string("UDP"), "Incorrect protocol name for UDP");
    ASSERT_EQUAL(getProtocolName(58), std::string("ICMPv6"), "Incorrect protocol name for ICMPv6");
    ASSERT_EQUAL(getProtocolName(255), std::string("Unknown"), "Incorrect protocol name for unknown");

    TEST_PASS();
    return true;
}

/**
 * Test: isAdministrator function works
 */
bool test_IsAdministrator_Works() {
    TEST_START("isAdministrator - Works");

    bool isAdmin = isAdministrator();

    // Function should return true or false (not crash)
    // We can't assert the value since it depends on how the test is run
    std::cout << "(Running as admin: " << (isAdmin ? "Yes" : "No") << ") ";

    TEST_PASS();
    return true;
}

/**
 * Test: All format functions work on same packet
 */
bool test_AllFormats_WorkOnSamePacket() {
    TEST_START("All formats - Work on same packet");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);

    // All format functions should work
    std::string json = formatPacketAsJson(packet);
    std::string text = formatPacketAsPlainText(packet);
    std::string hexDump = formatPacketAsHexDump(buffer, length);

    ASSERT_FALSE(json.empty(), "JSON is empty");
    ASSERT_FALSE(text.empty(), "Plain text is empty");
    ASSERT_FALSE(hexDump.empty(), "Hex dump is empty");

    TEST_PASS();
    return true;
}

/**
 * Test: Different formats contain same IP addresses
 */
bool test_DifferentFormats_ContainSameIpAddresses() {
    TEST_START("Different formats - Contain same IP addresses");

    unsigned char buffer[256];
    unsigned int length = 0;
    createMockTcpPacket(buffer, length);

    pcap_pkthdr header;
    createMockPacketHeader(&header, length);

    PacketInfo packet = parsePacket(&header, buffer);
    std::string json = formatPacketAsJson(packet);
    std::string text = formatPacketAsPlainText(packet);

    // Both formats should contain the same IP addresses
    ASSERT_TRUE(json.find("192.168.1.100") != std::string::npos, "Source IP not in JSON");
    ASSERT_TRUE(json.find("8.8.8.8") != std::string::npos, "Dest IP not in JSON");
    ASSERT_TRUE(text.find("192.168.1.100") != std::string::npos, "Source IP not in plain text");
    ASSERT_TRUE(text.find("8.8.8.8") != std::string::npos, "Dest IP not in plain text");

    TEST_PASS();
    return true;
}

/**
 * Test: openCaptureInterface with Npcap SDK check
 */
bool test_OpenCaptureInterface_NpcapCheck() {
    TEST_START("openCaptureInterface - Npcap SDK check");

    PacketCaptureConfig config;
    config.interfaceName = "test_interface";

    char errorBuffer[256];
    pcap_t* handle = openCaptureInterface(config, errorBuffer);

    // Should return NULL without Npcap SDK
    ASSERT_TRUE(handle == nullptr, "Should return NULL without Npcap SDK");
    ASSERT_FALSE(std::string(errorBuffer).empty(), "Error buffer should contain message");

    TEST_PASS();
    return true;
}

int main() {
    // Enable console colors on Windows
    enableConsoleColors();

    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "Network Packet Reader Tests" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl << std::endl;

    initializeGlobalLogger("test_network_packet_reader.csv");

    // Run all tests
    test_GetNetworkInterfaces_ReturnsList();
    test_ParsePacket_ValidTcpPacket();
    test_ParsePacket_ValidUdpPacket();
    test_ParsePacket_TruncatedPacket();
    test_FormatPacketAsJson_ReturnsValidJson();
    test_FormatPacketAsJson_IncludesIpAddresses();
    test_FormatPacketAsJson_ConsistentOutput();
    test_FormatPacketAsPlainText_ReturnsText();
    test_FormatPacketAsPlainText_ContainsStandardFields();
    test_FormatPacketAsPlainText_IncludesSeparators();
    test_FormatPacketAsPlainText_ShowsTcpFlags();
    test_FormatPacketAsHexDump_ReturnsHex();
    test_FormatPacketAsHexDump_CorrectFormat();
    test_GetProtocolName_ReturnsCorrectNames();
    test_IsAdministrator_Works();
    test_AllFormats_WorkOnSamePacket();
    test_DifferentFormats_ContainSameIpAddresses();
    test_OpenCaptureInterface_NpcapCheck();

    shutdownGlobalLogger();
    std::remove("test_network_packet_reader.csv");

    // Print summary
    std::cout << std::endl << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "Test Summary" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;

    int total = passed + failed + skipped;
    std::cout << "Total:   " << total << " tests" << std::endl;
    std::cout << COLOR_GREEN << "Passed:  " << passed << COLOR_RESET << std::endl;

    if (failed > 0) {
        std::cout << COLOR_RED << "Failed:  " << failed << COLOR_RESET << std::endl;
    } else {
        std::cout << "Failed:  " << failed << std::endl;
    }

    if (skipped > 0) {
        std::cout << COLOR_YELLOW << "Skipped: " << skipped << COLOR_RESET << std::endl;
    }

    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;

    if (failed == 0 && passed > 0) {
        std::cout << COLOR_GREEN << "All tests passed!" << COLOR_RESET << std::endl;
        return 0;
    } else if (failed > 0) {
        std::cout << COLOR_RED << "Some tests failed!" << COLOR_RESET << std::endl;
        return 1;
    } else {
        std::cout << COLOR_YELLOW << "No tests were run!" << COLOR_RESET << std::endl;
        return 1;
    }
}
