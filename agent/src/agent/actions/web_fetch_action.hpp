#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// Fetches a URL over HTTP/HTTPS.
// Uses cpp-httplib (httplib.h) if the header is found at compile time.
// Falls back to a curl-via-popen stub when httplib is not available.
// Thread-safe: each call is independent with no shared state.
class WebFetchAction : public Action {
public:
    WebFetchAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;

private:
    // curl fallback: returns {status_code, body}.
    static std::pair<int, std::string> curlFetch(const std::string& url,
                                                  const std::string& method,
                                                  const std::string& body,
                                                  const nlohmann::json& headers);
};

void registerWebFetchAction(WorkFactory& factory);

} // namespace agent
