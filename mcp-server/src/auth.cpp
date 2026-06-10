#include "auth.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <map>

using json = nlohmann::json;

static std::string getenv_or(const char* k, const char* d) {
    const char* v = std::getenv(k); return v ? v : d;
}

static const std::string INTROSPECT_URL =
    getenv_or("AUTH_INTROSPECT_URL", "http://localhost:8080/introspect");
static const std::string REQUIRED_AUDIENCE =
    getenv_or("MCP_SERVER_AUDIENCE", "http://localhost:8081");
static const std::string REQUIRED_SCOPE =
    getenv_or("MCP_REQUIRED_SCOPE", "tools:call");

// Parse "http[s]://host[:port][/path]" into {host, port, path}
static std::tuple<std::string,int,std::string> parse_url(const std::string& url) {
    auto s = url.find("://");
    if (s == std::string::npos) throw std::runtime_error("Bad URL: " + url);
    bool https = url.substr(0, s) == "https";
    std::string rest = url.substr(s + 3);
    auto sl = rest.find('/');
    std::string host_port = sl == std::string::npos ? rest : rest.substr(0, sl);
    std::string path      = sl == std::string::npos ? "/"  : rest.substr(sl);
    auto col = host_port.rfind(':');
    std::string host;
    int port;
    if (col != std::string::npos) {
        host = host_port.substr(0, col);
        port = std::stoi(host_port.substr(col + 1));
    } else {
        host = host_port;
        port = https ? 443 : 80;
    }
    return {host, port, path};
}

json validate_bearer_token(const std::string& bearer_token) {
    auto [host, port, path] = parse_url(INTROSPECT_URL);

    httplib::Client cli(host, port);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(10);

    std::string body = "token=" + bearer_token + "&token_type_hint=access_token";
    auto res = cli.Post(path.c_str(), body, "application/x-www-form-urlencoded");

    if (!res) throw std::runtime_error("Auth server unreachable");
    if (res->status != 200) throw std::runtime_error("Auth server returned " + std::to_string(res->status));

    auto claims = json::parse(res->body);
    if (!claims.value("active", false))
        throw std::runtime_error("Token inactive or expired");

    // Validate audience
    std::string aud = claims.value("aud", std::string{});
    if (aud != REQUIRED_AUDIENCE)
        throw std::runtime_error("Token audience mismatch");

    // Validate scope — all required scopes must be present
    std::string token_scope = claims.value("scope", std::string{});
    std::istringstream req_ss(REQUIRED_SCOPE);
    std::string required;
    while (req_ss >> required) {
        if (token_scope.find(required) == std::string::npos)
            throw std::runtime_error("Insufficient scope — missing: " + required);
    }

    return claims;
}
