#pragma once
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace crypto {

// ── Memory-managed wrappers ───────────────────────────────────────────────────

struct EvpKeyDeleter { void operator()(EVP_PKEY* k) const { if (k) EVP_PKEY_free(k); } };
using EvpKeyPtr = std::unique_ptr<EVP_PKEY, EvpKeyDeleter>;

// ── Encoding ─────────────────────────────────────────────────────────────────

std::string b64url_encode(const uint8_t* data, size_t len);
std::string b64url_encode(const std::vector<uint8_t>& v);
std::vector<uint8_t> b64url_decode(const std::string& s);
std::string b64url_encode_str(const std::string& s);

std::string url_decode(const std::string& s);

// ── Hashing ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> sha256(const std::string& data);

// ── Secure random ─────────────────────────────────────────────────────────────

std::vector<uint8_t> random_bytes(size_t n);
std::string random_token(size_t n = 32); // -> n random bytes, base64url-encoded

// ── RSA-2048 key generation ───────────────────────────────────────────────────

EvpKeyPtr generate_rsa_key(int bits = 2048);

// ── JWT RS256 ─────────────────────────────────────────────────────────────────

struct JwtClaims {
    std::string iss, aud, sub, jti, scope, client_id;
    int64_t     iat{0}, exp{0};
};

std::string  create_jwt(const JwtClaims& claims, EVP_PKEY* private_key);
JwtClaims    verify_jwt(const std::string& token,
                        EVP_PKEY* public_key,
                        const std::string& expected_iss,
                        const std::string& expected_aud);

// ── JWKS ──────────────────────────────────────────────────────────────────────

nlohmann::json public_key_to_jwk(EVP_PKEY* key, const std::string& kid = "1");

// ── PKCE S256 (RFC 7636) ──────────────────────────────────────────────────────

std::string  pkce_challenge_s256(const std::string& verifier);
bool         pkce_verify(const std::string& verifier,
                         const std::string& challenge,
                         const std::string& method);

} // namespace crypto
