/**
 * @file json_utils.cpp
 * @brief Implementation of JSON utility functions
 *
 * Provides string escaping and formatting for JSON compliance.
 */

#include "json_utils.h" // escapeJson declarations
#include <sstream>       // std::ostringstream
#include <iomanip>       // std::setw, std::setfill, std::hex

std::string escapeJson(const std::string& str) {
    std::ostringstream oss;

    // Iterate through each character and escape as needed
    for (char c : str) {
        switch (c) {
            case '"':   oss << "\\\""; break;  // Quotation mark
            case '\\':  oss << "\\\\"; break;  // Backslash
            case '\b':  oss << "\\b";  break;  // Backspace
            case '\f':  oss << "\\f";  break;  // Form feed
            case '\n':  oss << "\\n";  break;  // Newline
            case '\r':  oss << "\\r";  break;  // Carriage return
            case '\t':  oss << "\\t";  break;  // Tab
            default:
                // Escape control characters (0x00-0x1F) as Unicode
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c);
                } else {
                    // Regular character, no escaping needed
                    oss << c;
                }
        }
    }

    return oss.str();
}
