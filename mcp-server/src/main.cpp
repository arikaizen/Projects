// MCP Server — Streamable HTTP + stdio transports
//
// HTTP endpoint: POST /mcp/v1  (JSON-RPC 2.0, Bearer auth via auth-server)
// stdio mode:    STDIN_STDIO=1  (newline-delimited JSON-RPC, no auth)
//
// Tools are loaded from tools_manifest.json and executed as Python subprocesses.

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "auth.hpp"
#include "tool_registry.hpp"
#include "tool_runner.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string getenv_or(const char* k, const std::string& d) {
    const char* v = std::getenv(k); return v ? std::string(v) : d;
}

// ── Global state ──────────────────────────────────────────────────────────────
static ToolRegistry  g_registry;
static std::unique_ptr<ToolRunner> g_runner;

// ── JSON-RPC helpers ──────────────────────────────────────────────────────────

static json rpc_ok(const json& id, const json& result) {
    return {{"jsonrpc","2.0"},{"id",id},{"result",result}};
}
static json rpc_err(const json& id, int code, const std::string& msg) {
    return {{"jsonrpc","2.0"},{"id",id},
            {"error",{{"code",code},{"message",msg}}}};
}

// ── Dispatch a single JSON-RPC 2.0 request ───────────────────────────────────

static json dispatch(const json& req) {
    if (req.value("jsonrpc","") != "2.0")
        return rpc_err(nullptr, -32600, "jsonrpc must be '2.0'");

    auto id     = req.value("id",     json{nullptr});
    auto method = req.value("method", std::string{});
    auto params = req.value("params", json::object());

    if (method == "ping")
        return rpc_ok(id, {{"pong", true}});

    if (method == "tools/list") {
        // Optional filter: {"category": "weather"}
        std::string cat = params.value("category", std::string{});
        return rpc_ok(id, {{"tools", g_registry.mcp_tools_list()}});
    }

    if (method == "tools/call") {
        std::string name = params.value("name", std::string{});
        auto        args = params.value("arguments", json::object());

        const ToolDef* td = g_registry.find(name);
        if (!td) return rpc_err(id, -32601, "tool not found: " + name);

        auto result = g_runner->run(td->script, args);
        if (!result.success)
            return rpc_err(id, -32000, result.error);

        return rpc_ok(id, {
            {"content", json::array({{{"type","text"},
                                      {"text", result.result.is_string()
                                               ? result.result.get<std::string>()
                                               : result.result.dump()}}})},
            {"category", td->category},
        });
    }

    return rpc_err(id, -32601, "method not found: " + method);
}

// ── stdio transport (local subprocess, no auth) ───────────────────────────────

static void run_stdio() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        json resp;
        try {
            auto req = json::parse(line);
            resp = dispatch(req);
        } catch (const std::exception& e) {
            resp = rpc_err(nullptr, -32700, std::string("Parse error: ") + e.what());
        }
        std::cout << resp.dump() << "\n";
        std::cout.flush();
    }
}

// ── HTTP transport ────────────────────────────────────────────────────────────

static void cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
}

static void handle_mcp(const httplib::Request& req, httplib::Response& res) {
    cors(res);

    // Validate Bearer token (step ④)
    auto auth_hdr = req.get_header_value("Authorization");
    if (auth_hdr.size() < 8 || auth_hdr.substr(0, 7) != "Bearer ") {
        res.status = 401;
        res.set_header("WWW-Authenticate", "Bearer");
        res.set_content(json{{"error","Bearer token required"}}.dump(), "application/json");
        return;
    }
    try {
        validate_bearer_token(auth_hdr.substr(7));
    } catch (const std::exception& e) {
        res.status = 401;
        res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        return;
    }

    // Parse and dispatch JSON-RPC
    json body;
    try { body = json::parse(req.body); }
    catch (...) {
        res.status = 400;
        res.set_content(rpc_err(nullptr, -32700, "Parse error").dump(), "application/json");
        return;
    }

    json resp;
    if (body.is_array()) {
        resp = json::array();
        for (auto& item : body) resp.push_back(dispatch(item));
    } else {
        resp = dispatch(body);
    }
    res.status = 200;
    res.set_content(resp.dump(), "application/json");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    // Paths
    fs::path exe_dir   = fs::current_path();
    fs::path tools_dir = fs::path(getenv_or("TOOLS_DIR",
                             (exe_dir / "../tools").string()));
    fs::path manifest  = fs::path(getenv_or("TOOLS_MANIFEST",
                             (tools_dir / "tools_manifest.json").string()));

    std::cout << "[mcp-server] Loading tool manifest: " << manifest << "\n";
    try {
        g_registry.load(manifest);
        g_registry.set_tools_dir(tools_dir);
    } catch (const std::exception& e) {
        std::cerr << "[mcp-server] FATAL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "[mcp-server] " << g_registry.list().size() << " tools loaded.\n";

    g_runner = std::make_unique<ToolRunner>(tools_dir);

    // stdio mode
    if (std::getenv("STDIN_STDIO")) {
        std::cerr << "[mcp-server] stdio mode\n";
        run_stdio();
        return 0;
    }

    // HTTP mode
    std::string host = getenv_or("HOST", "0.0.0.0");
    int port = std::atoi(getenv_or("PORT", "8081").c_str());

    httplib::Server svr;
    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        cors(res); return httplib::Server::HandlerResponse::Unhandled;
    });
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        cors(res); res.status = 204;
    });

    svr.Post("/mcp/v1", handle_mcp);

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        cors(res);
        res.set_content(json{{"status","ok"},
                             {"tools", static_cast<int>(g_registry.list().size())}
                            }.dump(), "application/json");
    });

    std::cout << "[mcp-server] Listening on " << host << ":" << port << "\n";
    svr.listen(host.c_str(), port);
    return 0;
}
