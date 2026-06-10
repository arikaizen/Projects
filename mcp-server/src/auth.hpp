#pragma once
#include <nlohmann/json.hpp>
#include <string>

// Calls the auth-server's /introspect endpoint and validates audience + scope.
// Returns the active token claims on success; throws std::runtime_error on failure.
nlohmann::json validate_bearer_token(const std::string& bearer_token);
