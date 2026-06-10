#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <ctime>

// ── Data models ───────────────────────────────────────────────────────────────

struct Client {
    std::string              id;
    std::string              name;
    std::vector<std::string> redirect_uris;
    std::vector<std::string> grant_types;
    std::string              scope;
};

struct AuthCode {
    std::string code;
    std::string client_id;
    std::string redirect_uri;
    std::string scope;
    std::string code_challenge;
    std::string code_challenge_method;
    std::time_t expires_at{0};
    bool        used{false};
};

struct AccessToken {
    std::string jti;
    std::string client_id;
    std::string scope;
    std::time_t issued_at{0};
    std::time_t expires_at{0};
    bool        revoked{false};
};

struct RefreshToken {
    std::string token;
    std::string client_id;
    std::string scope;
    std::time_t issued_at{0};
    std::time_t expires_at{0};
    bool        revoked{false};
};

// ── Thread-safe in-memory store ───────────────────────────────────────────────

class Store {
public:
    // ── Clients ───────────────────────────────────────────────────────────────
    Client add_client(std::string name,
                      std::vector<std::string> redirect_uris,
                      std::vector<std::string> grant_types,
                      std::string scope);

    std::optional<Client> get_client(const std::string& id) const;

    // ── Auth codes ────────────────────────────────────────────────────────────
    void save_code(AuthCode code);
    // Returns the code and marks it used; returns nullopt if not found / used.
    std::optional<AuthCode> consume_code(const std::string& code);

    // ── Access tokens ─────────────────────────────────────────────────────────
    void save_token(AccessToken token);
    std::optional<AccessToken> get_token(const std::string& jti) const;
    void revoke_token(const std::string& jti);

    // ── Refresh tokens ────────────────────────────────────────────────────────
    void save_refresh(RefreshToken rt);
    std::optional<RefreshToken> consume_refresh(const std::string& token);

private:
    mutable std::mutex m_clients_mu, m_codes_mu, m_tokens_mu, m_refresh_mu;
    std::unordered_map<std::string, Client>       m_clients;
    std::unordered_map<std::string, AuthCode>     m_codes;
    std::unordered_map<std::string, AccessToken>  m_tokens;
    std::unordered_map<std::string, RefreshToken> m_refresh;
};
