#include "store.hpp"
#include "crypto.hpp"
#include <algorithm>

Client Store::add_client(std::string name,
                         std::vector<std::string> redirect_uris,
                         std::vector<std::string> grant_types,
                         std::string scope) {
    Client c;
    c.id           = "client_" + crypto::random_token(12);
    c.name         = std::move(name);
    c.redirect_uris = std::move(redirect_uris);
    c.grant_types  = std::move(grant_types);
    c.scope        = std::move(scope);
    std::lock_guard<std::mutex> lk(m_clients_mu);
    m_clients[c.id] = c;
    return c;
}

std::optional<Client> Store::get_client(const std::string& id) const {
    std::lock_guard<std::mutex> lk(m_clients_mu);
    auto it = m_clients.find(id);
    if (it == m_clients.end()) return std::nullopt;
    return it->second;
}

void Store::save_code(AuthCode code) {
    std::lock_guard<std::mutex> lk(m_codes_mu);
    m_codes[code.code] = std::move(code);
}

std::optional<AuthCode> Store::consume_code(const std::string& code) {
    std::lock_guard<std::mutex> lk(m_codes_mu);
    auto it = m_codes.find(code);
    if (it == m_codes.end() || it->second.used) return std::nullopt;
    it->second.used = true;
    return it->second;
}

void Store::save_token(AccessToken token) {
    std::lock_guard<std::mutex> lk(m_tokens_mu);
    m_tokens[token.jti] = std::move(token);
}

std::optional<AccessToken> Store::get_token(const std::string& jti) const {
    std::lock_guard<std::mutex> lk(m_tokens_mu);
    auto it = m_tokens.find(jti);
    if (it == m_tokens.end()) return std::nullopt;
    return it->second;
}

void Store::revoke_token(const std::string& jti) {
    std::lock_guard<std::mutex> lk(m_tokens_mu);
    auto it = m_tokens.find(jti);
    if (it != m_tokens.end()) it->second.revoked = true;
}

void Store::save_refresh(RefreshToken rt) {
    std::lock_guard<std::mutex> lk(m_refresh_mu);
    m_refresh[rt.token] = std::move(rt);
}

std::optional<RefreshToken> Store::consume_refresh(const std::string& token) {
    std::lock_guard<std::mutex> lk(m_refresh_mu);
    auto it = m_refresh.find(token);
    if (it == m_refresh.end() || it->second.revoked) return std::nullopt;
    it->second.revoked = true;
    return it->second;
}
