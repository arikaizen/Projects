#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>

namespace protocol {

struct Request {
    std::string command;
    std::vector<std::string> args;
};

Request parseRequest(const std::string& raw);
std::string formatOK(const std::string& json);
std::string formatError(const std::string& message);
std::string jsonEscape(const std::string& s);

} // namespace protocol

#endif // PROTOCOL_H
