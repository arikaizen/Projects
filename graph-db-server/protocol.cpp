#include "protocol.h"

#include <cstdio>

namespace protocol {

Request parseRequest(const std::string& raw) {
    Request req;

    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return req;
    size_t end = raw.find_last_not_of(" \t\r\n");
    std::string trimmed = raw.substr(start, end - start + 1);

    size_t spacePos = trimmed.find(' ');
    if (spacePos == std::string::npos) {
        req.command = trimmed;
        return req;
    }

    req.command = trimmed.substr(0, spacePos);
    std::string rest = trimmed.substr(spacePos + 1);

    size_t pos = 0;
    while (true) {
        size_t sep = rest.find(" | ", pos);
        if (sep == std::string::npos) {
            req.args.push_back(rest.substr(pos));
            break;
        }
        req.args.push_back(rest.substr(pos, sep - pos));
        pos = sep + 3;
    }
    return req;
}

std::string formatOK(const std::string& json) {
    return "OK " + json + "\n";
}

std::string formatError(const std::string& message) {
    return "ERROR " + message + "\n";
}

std::string jsonEscape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }
    return result;
}

} // namespace protocol
