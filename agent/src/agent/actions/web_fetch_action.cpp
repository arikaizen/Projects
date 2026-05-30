#include "web_fetch_action.hpp"
#include "agent/agent_context.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

// Conditionally use cpp-httplib when available.
#if __has_include(<httplib.h>)
#  define AGENT_HAS_HTTPLIB 1
#  include <httplib.h>
#else
#  define AGENT_HAS_HTTPLIB 0
#endif

namespace agent {

// ── curl fallback ────────────────────────────────────────────────────────────

std::pair<int, std::string> WebFetchAction::curlFetch(const std::string&    url,
                                                       const std::string&    method,
                                                       const std::string&    body,
                                                       const nlohmann::json& headers) {
    // Build curl invocation.
    // We capture body first, then a separator line, then the HTTP status code.
    std::string cmd = "curl -s -L";
    cmd += " -X " + method;

    // Add headers.
    for (auto& [key, val] : headers.items()) {
        std::string hval = val.is_string() ? val.get<std::string>() : val.dump();
        // Minimal shell escaping: wrap in single quotes, escape internal single quotes.
        auto escape = [](const std::string& s) {
            std::string out = "'";
            for (char c : s) {
                if (c == '\'') out += "'\\''";
                else           out += c;
            }
            out += "'";
            return out;
        };
        cmd += " -H " + escape(key + ": " + hval);
    }

    if (!body.empty()) {
        // Write body to a temp file to avoid shell quoting nightmares.
        // For simplicity we embed via --data-raw; length is bounded by what
        // agents typically send (a few KB).
        cmd += " --data-raw ''";  // placeholder — replaced below if body non-empty.
        // Actually use a pipe via printf is cleaner:
        // We reconstruct command properly.
        cmd = "curl -s -L -X " + method;
        for (auto& [key, val] : headers.items()) {
            std::string hval = val.is_string() ? val.get<std::string>() : val.dump();
            cmd += " -H '" + key + ": " + hval + "'";
        }
        // Write body via stdin.
        std::string safe_body;
        for (char c : body) {
            if (c == '\'') safe_body += "'\\''";
            else           safe_body += c;
        }
        cmd = "printf '%s' '" + safe_body + "' | curl -s -L -X " + method + " --data-binary @-";
        for (auto& [key, val] : headers.items()) {
            std::string hval = val.is_string() ? val.get<std::string>() : val.dump();
            cmd += " -H '" + key + ": " + hval + "'";
        }
        cmd += " -w '\\n__STATUS__%{http_code}' '" + url + "' 2>/dev/null";
    } else {
        cmd += " -w '\\n__STATUS__%{http_code}' '" + url + "' 2>/dev/null";
    }

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return {-1, "popen failed"};
    }

    std::array<char, 4096> buf{};
    std::string raw;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        raw += buf.data();
    }
    pclose(pipe);

    // Extract status code from the sentinel.
    const std::string sentinel = "\n__STATUS__";
    auto pos = raw.rfind(sentinel);
    std::string response_body = raw;
    int status_code = 0;
    if (pos != std::string::npos) {
        std::string code_str = raw.substr(pos + sentinel.size());
        try { status_code = std::stoi(code_str); } catch (...) {}
        response_body = raw.substr(0, pos);
    }

    return {status_code, response_body};
}

// ── execute ──────────────────────────────────────────────────────────────────

WorkResult WebFetchAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved = ctx.resolveReferences(inputs);
        auto url      = resolved.at("url").get<std::string>();
        auto method   = resolved.value("method", std::string("GET"));
        auto body     = resolved.value("body",   std::string(""));
        auto headers  = resolved.contains("headers") && resolved["headers"].is_object()
                            ? resolved["headers"]
                            : nlohmann::json::object();

        std::cerr << "[ACTION:" << name << "] " << method << " " << url << "\n";

        int         status_code = 0;
        std::string response_body;

#if AGENT_HAS_HTTPLIB
        // Parse URL into scheme, host, path.
        std::string host, path_part;
        bool        use_https = false;
        auto strip_scheme = [&](const std::string& u, const std::string& scheme, bool https) {
            if (u.rfind(scheme, 0) == 0) {
                use_https = https;
                std::string rest = u.substr(scheme.size());
                auto slash = rest.find('/');
                if (slash == std::string::npos) {
                    host = rest;
                    path_part = "/";
                } else {
                    host = rest.substr(0, slash);
                    path_part = rest.substr(slash);
                }
                return true;
            }
            return false;
        };

        bool parsed = strip_scheme(url, "https://", true) || strip_scheme(url, "http://", false);

        if (parsed) {
            auto make_request = [&](auto& cli) {
                httplib::Headers h;
                for (auto& [k, v] : headers.items()) {
                    h.emplace(k, v.is_string() ? v.get<std::string>() : v.dump());
                }
                httplib::Result res;
                if (method == "GET") {
                    res = cli.Get(path_part.c_str(), h);
                } else if (method == "POST") {
                    std::string ct = "application/json";
                    res = cli.Post(path_part.c_str(), h, body, ct.c_str());
                } else if (method == "PUT") {
                    res = cli.Put(path_part.c_str(), h, body, "application/json");
                } else if (method == "DELETE") {
                    res = cli.Delete(path_part.c_str(), h);
                } else {
                    // Generic fallback via GET.
                    res = cli.Get(path_part.c_str(), h);
                }
                if (res) {
                    status_code   = res->status;
                    response_body = res->body;
                } else {
                    status_code   = -1;
                    response_body = "httplib error: " + httplib::to_string(res.error());
                }
            };

            if (use_https) {
                httplib::SSLClient cli(host.c_str());
                cli.enable_server_certificate_verification(false);
                make_request(cli);
            } else {
                httplib::Client cli(host.c_str());
                make_request(cli);
            }
        } else {
            // Unparseable URL — fall back to curl.
            auto [sc, rb] = curlFetch(url, method, body, headers);
            status_code   = sc;
            response_body = rb;
        }
#else
        // No httplib — use curl.
        auto [sc, rb] = curlFetch(url, method, body, headers);
        status_code   = sc;
        response_body = rb;
#endif

        result.success = (status_code >= 200 && status_code < 300);
        result.output  = {{"status_code", status_code}, {"body", response_body}, {"url", url}};
        if (!result.success) {
            result.error = "HTTP " + std::to_string(status_code);
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerWebFetchAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "WebFetchAction",
        "Fetch a URL via HTTP/HTTPS. Uses cpp-httplib if available, otherwise falls back to curl.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"url"}},
            {"properties", {
                {"url",     {{"type", "string"}, {"description", "URL to fetch."}}},
                {"method",  {{"type", "string"}, {"description", "HTTP method (default GET)."}}},
                {"headers", {{"type", "object"}, {"description", "Optional HTTP headers."}}},
                {"body",    {{"type", "string"}, {"description", "Optional request body."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<WebFetchAction>(std::move(id), "WebFetchAction", std::move(inputs));
    });
}

} // namespace agent
