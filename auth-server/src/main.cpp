// OAuth 2.1 Authorization Server
// Endpoints:
//   POST /register        — RFC 7591 dynamic client registration
//   GET  /authorize       — PKCE S256 authorization endpoint
//   POST /token           — token endpoint (authorization_code, refresh_token)
//   POST /introspect      — RFC 7662 token introspection (used by MCP server)
//   GET  /.well-known/oauth-authorization-server
//   GET  /.well-known/jwks.json
//   GET  /health

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "crypto.hpp"
#include "store.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <map>

using json = nlohmann::json;

// ── Config from environment ───────────────────────────────────────────────────

static std::string getenv_or(const char* name, const char* def) {
    const char* v = std::getenv(name);
    return v ? v : def;
}

static const std::string ISSUER   = getenv_or("AUTH_SERVER_ISSUER", "http://localhost:8080");
static const std::string AUDIENCE = getenv_or("MCP_SERVER_AUDIENCE", "http://localhost:8081");
static const int ACCESS_TTL       = std::atoi(getenv_or("ACCESS_TOKEN_TTL",  "3600").c_str());
static const int REFRESH_TTL      = std::atoi(getenv_or("REFRESH_TOKEN_TTL", "86400").c_str());

// ── Global state (initialized once at startup) ────────────────────────────────

static Store g_store;
static crypto::EvpKeyPtr g_private_key;

// ── Utilities ─────────────────────────────────────────────────────────────────

static std::map<std::string,std::string> parse_form(const std::string& body) {
    std::map<std::string,std::string> m;
    std::istringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos)
            m[crypto::url_decode(pair.substr(0,eq))] =
              crypto::url_decode(pair.substr(eq+1));
    }
    return m;
}

static void cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Authorization, Content-Type");
}

static void json_resp(httplib::Response& res, int status, const json& body) {
    cors(res);
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

static void error_resp(httplib::Response& res, int status,
                        const std::string& error,
                        const std::string& desc = "") {
    json body = {{"error", error}};
    if (!desc.empty()) body["error_description"] = desc;
    json_resp(res, status, body);
}

// ── Handlers ──────────────────────────────────────────────────────────────────

static void handle_register(const httplib::Request& req, httplib::Response& res) {
    json body;
    try { body = json::parse(req.body); }
    catch (...) { return error_resp(res, 400, "invalid_request", "JSON parse error"); }

    std::string name  = body.value("client_name", std::string{});
    std::string scope = body.value("scope",        std::string{"tools:read tools:call"});
    if (name.empty()) return error_resp(res, 400, "invalid_request", "client_name required");

    std::vector<std::string> uris, grants;
    for (auto& u : body.value("redirect_uris",  json::array())) uris.push_back(u);
    for (auto& g : body.value("grant_types",     json::array())) grants.push_back(g);
    if (uris.empty()) return error_resp(res, 400, "invalid_request", "redirect_uris required");
    if (grants.empty()) grants = {"authorization_code", "refresh_token"};

    auto client = g_store.add_client(name, uris, grants, scope);
    json_resp(res, 201, {
        {"client_id",      client.id},
        {"client_name",    client.name},
        {"redirect_uris",  client.redirect_uris},
        {"grant_types",    client.grant_types},
        {"scope",          client.scope},
        {"token_endpoint_auth_method", "none"},
    });
}

static void handle_authorize(const httplib::Request& req, httplib::Response& res) {
    auto get = [&](const char* k) { return req.get_param_value(k); };
    std::string rt     = get("response_type");
    std::string cid    = get("client_id");
    std::string redir  = get("redirect_uri");
    std::string scope  = get("scope");    if (scope.empty()) scope = "tools:read tools:call";
    std::string state  = get("state");
    std::string cc     = get("code_challenge");
    std::string ccm    = get("code_challenge_method"); if (ccm.empty()) ccm = "S256";

    if (rt  != "code")  return error_resp(res, 400, "unsupported_response_type");
    if (ccm != "S256")  return error_resp(res, 400, "unsupported_code_challenge_method");
    if (cc.empty())     return error_resp(res, 400, "invalid_request", "code_challenge required");

    auto client = g_store.get_client(cid);
    if (!client) return error_resp(res, 400, "invalid_client");
    bool redir_ok = false;
    for (auto& u : client->redirect_uris) if (u == redir) { redir_ok = true; break; }
    if (!redir_ok) return error_resp(res, 400, "invalid_redirect_uri");

    AuthCode code;
    code.code                  = crypto::random_token(24);
    code.client_id             = cid;
    code.redirect_uri          = redir;
    code.scope                 = scope;
    code.code_challenge        = cc;
    code.code_challenge_method = ccm;
    code.expires_at            = std::time(nullptr) + 600;
    g_store.save_code(code);

    std::string location = redir + (redir.find('?') == std::string::npos ? "?" : "&");
    location += "code=" + code.code;
    if (!state.empty()) location += "&state=" + state;
    cors(res);
    res.set_redirect(location, 302);
}

static void handle_token(const httplib::Request& req, httplib::Response& res) {
    auto form = parse_form(req.body);
    auto f    = [&](const char* k) -> std::string {
        auto it = form.find(k); return it == form.end() ? "" : it->second; };

    std::string grant_type    = f("grant_type");
    std::string client_id     = f("client_id");
    std::string redirect_uri  = f("redirect_uri");
    std::string code          = f("code");
    std::string code_verifier = f("code_verifier");
    std::string refresh_tok   = f("refresh_token");

    auto client = g_store.get_client(client_id);
    if (!client) return error_resp(res, 400, "invalid_client");

    auto now = std::time(nullptr);
    auto issue_tokens = [&](const std::string& scope) {
        crypto::JwtClaims c;
        c.iss       = ISSUER;
        c.aud       = AUDIENCE;
        c.sub       = client_id;
        c.client_id = client_id;
        c.scope     = scope;
        c.jti       = crypto::random_token(16);
        c.iat       = now;
        c.exp       = now + ACCESS_TTL;
        std::string jwt = crypto::create_jwt(c, g_private_key.get());

        AccessToken at;
        at.jti       = c.jti;
        at.client_id = client_id;
        at.scope     = scope;
        at.issued_at = now;
        at.expires_at = c.exp;
        g_store.save_token(at);

        RefreshToken rt;
        rt.token     = crypto::random_token(32);
        rt.client_id = client_id;
        rt.scope     = scope;
        rt.issued_at = now;
        rt.expires_at = now + REFRESH_TTL;
        g_store.save_refresh(rt);

        json_resp(res, 200, {
            {"access_token",  jwt},
            {"token_type",    "Bearer"},
            {"expires_in",    ACCESS_TTL},
            {"refresh_token", rt.token},
            {"scope",         scope},
        });
    };

    if (grant_type == "authorization_code") {
        if (code.empty())          return error_resp(res, 400, "invalid_request", "code required");
        if (code_verifier.empty()) return error_resp(res, 400, "invalid_request", "code_verifier required");

        auto auth_code = g_store.consume_code(code);
        if (!auth_code) return error_resp(res, 400, "invalid_grant", "code not found or already used");
        if (std::time(nullptr) > auth_code->expires_at)
            return error_resp(res, 400, "invalid_grant", "code expired");
        if (auth_code->client_id != client_id)
            return error_resp(res, 400, "invalid_grant");
        if (!redirect_uri.empty() && auth_code->redirect_uri != redirect_uri)
            return error_resp(res, 400, "invalid_grant", "redirect_uri mismatch");
        if (!crypto::pkce_verify(code_verifier, auth_code->code_challenge,
                                  auth_code->code_challenge_method))
            return error_resp(res, 400, "invalid_grant", "PKCE verification failed");
        return issue_tokens(auth_code->scope);
    }

    if (grant_type == "refresh_token") {
        if (refresh_tok.empty()) return error_resp(res, 400, "invalid_request", "refresh_token required");
        auto rt = g_store.consume_refresh(refresh_tok);
        if (!rt) return error_resp(res, 400, "invalid_grant", "refresh_token not found or revoked");
        if (std::time(nullptr) > rt->expires_at) return error_resp(res, 400, "invalid_grant", "refresh_token expired");
        if (rt->client_id != client_id) return error_resp(res, 400, "invalid_grant");
        return issue_tokens(rt->scope);
    }

    error_resp(res, 400, "unsupported_grant_type");
}

static void handle_introspect(const httplib::Request& req, httplib::Response& res) {
    auto form  = parse_form(req.body);
    auto it    = form.find("token");
    if (it == form.end()) { json_resp(res, 200, {{"active", false}}); return; }

    auto inactive = [&]{ json_resp(res, 200, {{"active", false}}); };

    crypto::JwtClaims claims;
    try {
        claims = crypto::verify_jwt(it->second, g_private_key.get(), ISSUER, AUDIENCE);
    } catch (...) { return inactive(); }

    auto token_rec = g_store.get_token(claims.jti);
    if (!token_rec || token_rec->revoked) return inactive();
    if (std::time(nullptr) > token_rec->expires_at) return inactive();

    json_resp(res, 200, {
        {"active",     true},
        {"scope",      token_rec->scope},
        {"client_id",  token_rec->client_id},
        {"sub",        claims.sub},
        {"aud",        AUDIENCE},
        {"iss",        ISSUER},
        {"exp",        static_cast<int64_t>(token_rec->expires_at)},
        {"iat",        static_cast<int64_t>(token_rec->issued_at)},
        {"jti",        token_rec->jti},
        {"token_type", "Bearer"},
    });
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    std::string host = getenv_or("HOST", "0.0.0.0");
    int         port = std::atoi(getenv_or("PORT", "8080").c_str());

    std::cout << "[auth-server] Generating RSA-2048 key pair...\n";
    g_private_key = crypto::generate_rsa_key(2048);
    std::cout << "[auth-server] Keys ready.\n";

    httplib::Server svr;
    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        cors(res);
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        cors(res); res.status = 204;
    });

    svr.Post("/register",   handle_register);
    svr.Get ("/authorize",  handle_authorize);
    svr.Post("/token",      handle_token);
    svr.Post("/introspect", handle_introspect);

    svr.Get("/.well-known/oauth-authorization-server", [&](const httplib::Request&, httplib::Response& res) {
        json_resp(res, 200, {
            {"issuer",                                  ISSUER},
            {"authorization_endpoint",                  ISSUER + "/authorize"},
            {"token_endpoint",                          ISSUER + "/token"},
            {"introspection_endpoint",                  ISSUER + "/introspect"},
            {"registration_endpoint",                   ISSUER + "/register"},
            {"jwks_uri",                                ISSUER + "/.well-known/jwks.json"},
            {"response_types_supported",                json::array({"code"})},
            {"grant_types_supported",                   json::array({"authorization_code","refresh_token"})},
            {"code_challenge_methods_supported",        json::array({"S256"})},
            {"token_endpoint_auth_methods_supported",   json::array({"none"})},
            {"scopes_supported",                        json::array({"tools:read","tools:call"})},
        });
    });

    svr.Get("/.well-known/jwks.json", [&](const httplib::Request&, httplib::Response& res) {
        json_resp(res, 200, {{"keys", json::array({
            crypto::public_key_to_jwk(g_private_key.get(), "1")
        })}});
    });

    svr.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        json_resp(res, 200, {{"status","ok"},{"issuer",ISSUER},{"audience",AUDIENCE}});
    });

    std::cout << "[auth-server] Listening on " << host << ":" << port << "\n";
    svr.listen(host.c_str(), port);
    return 0;
}
