#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

enum class Role : uint8_t { System, User, Assistant };

struct Message {
    Role        role;
    std::string content;
};

inline std::string RoleToStr(Role r) {
    switch (r) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
    }
    throw std::invalid_argument("RoleToStr: unknown Role");
}

inline Role RoleFromStr(const std::string& s) {
    if (s == "system")    return Role::System;
    if (s == "user")      return Role::User;
    if (s == "assistant") return Role::Assistant;
    throw std::invalid_argument("RoleFromStr: unknown role \"" + s + "\"");
}
