#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace crypto {

// ── base64url ─────────────────────────────────────────────────────────────────

static const char B64URL_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string b64url_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; ) {
        uint32_t b = static_cast<uint8_t>(data[i++]) << 16;
        if (i < len) b |= static_cast<uint8_t>(data[i++]) << 8;
        if (i < len) b |= static_cast<uint8_t>(data[i++]);
        out += B64URL_TABLE[(b >> 18) & 0x3F];
        out += B64URL_TABLE[(b >> 12) & 0x3F];
        out += B64URL_TABLE[(b >>  6) & 0x3F];
        out += B64URL_TABLE[(b      ) & 0x3F];
    }
    // remove padding
    size_t pad = (3 - len % 3) % 3;
    out.resize(out.size() - pad);
    return out;
}

std::string b64url_encode(const std::vector<uint8_t>& v) {
    return b64url_encode(v.data(), v.size());
}

std::string b64url_encode_str(const std::string& s) {
    return b64url_encode(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

static int b64url_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-' || c == '+') return 62;
    if (c == '_' || c == '/') return 63;
    return -1;
}

std::vector<uint8_t> b64url_decode(const std::string& s) {
    std::string padded = s;
    while (padded.size() % 4) padded += '=';
    std::vector<uint8_t> out;
    out.reserve(padded.size() / 4 * 3);
    for (size_t i = 0; i < padded.size(); i += 4) {
        int a = b64url_char_value(padded[i]);
        int b = b64url_char_value(padded[i+1]);
        int c = padded[i+2] == '=' ? 0 : b64url_char_value(padded[i+2]);
        int d = padded[i+3] == '=' ? 0 : b64url_char_value(padded[i+3]);
        if (a < 0 || b < 0) break;
        out.push_back(static_cast<uint8_t>((a << 2) | (b >> 4)));
        if (padded[i+2] != '=') out.push_back(static_cast<uint8_t>(((b & 0xF) << 4) | (c >> 2)));
        if (padded[i+3] != '=') out.push_back(static_cast<uint8_t>(((c & 0x3) << 6) | d));
    }
    return out;
}

// ── URL decode ────────────────────────────────────────────────────────────────

std::string url_decode(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = {s[i+1], s[i+2], '\0'};
            result += static_cast<char>(std::strtol(hex, nullptr, 16));
            i += 3;
        } else if (s[i] == '+') {
            result += ' '; ++i;
        } else {
            result += s[i++];
        }
    }
    return result;
}

// ── SHA-256 ───────────────────────────────────────────────────────────────────

std::vector<uint8_t> sha256(const std::string& data) {
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    SHA256(reinterpret_cast<const uint8_t*>(data.data()), data.size(), digest.data());
    return digest;
}

// ── Secure random ─────────────────────────────────────────────────────────────

std::vector<uint8_t> random_bytes(size_t n) {
    std::vector<uint8_t> buf(n);
    if (RAND_bytes(buf.data(), static_cast<int>(n)) != 1)
        throw std::runtime_error("RAND_bytes failed");
    return buf;
}

std::string random_token(size_t n) {
    return b64url_encode(random_bytes(n));
}

// ── RSA key generation ────────────────────────────────────────────────────────

EvpKeyPtr generate_rsa_key(int bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx); throw std::runtime_error("EVP_PKEY_keygen_init failed");
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
        EVP_PKEY_CTX_free(ctx); throw std::runtime_error("EVP_PKEY_CTX_set_rsa_keygen_bits failed");
    }
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx); throw std::runtime_error("EVP_PKEY_keygen failed");
    }
    EVP_PKEY_CTX_free(ctx);
    return EvpKeyPtr{pkey};
}

// ── JWT RS256 ─────────────────────────────────────────────────────────────────

static std::string sign_rs256(const std::string& msg, EVP_PKEY* key) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, key) <= 0) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("EVP_DigestSignInit failed");
    }
    if (EVP_DigestSignUpdate(ctx, msg.data(), msg.size()) <= 0) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("EVP_DigestSignUpdate failed");
    }
    size_t sig_len = 0;
    EVP_DigestSignFinal(ctx, nullptr, &sig_len);
    std::vector<uint8_t> sig(sig_len);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sig_len) <= 0) {
        EVP_MD_CTX_free(ctx); throw std::runtime_error("EVP_DigestSignFinal failed");
    }
    EVP_MD_CTX_free(ctx);
    return b64url_encode(sig.data(), sig_len);
}

static bool verify_rs256(const std::string& msg,
                          const std::string& sig_b64,
                          EVP_PKEY* key) {
    auto sig = b64url_decode(sig_b64);
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, key) > 0 &&
        EVP_DigestVerifyUpdate(ctx, msg.data(), msg.size()) > 0 &&
        EVP_DigestVerifyFinal(ctx, sig.data(), sig.size()) == 1) {
        ok = true;
    }
    EVP_MD_CTX_free(ctx);
    return ok;
}

std::string create_jwt(const JwtClaims& c, EVP_PKEY* private_key) {
    using json = nlohmann::json;
    std::string header = b64url_encode_str(
        json{{"alg","RS256"},{"typ","JWT"},{"kid","1"}}.dump());
    std::string payload = b64url_encode_str(json{
        {"iss", c.iss}, {"aud", c.aud}, {"sub", c.sub},
        {"client_id", c.client_id}, {"scope", c.scope},
        {"jti", c.jti}, {"iat", c.iat}, {"exp", c.exp}
    }.dump());
    std::string signing_input = header + "." + payload;
    std::string sig = sign_rs256(signing_input, private_key);
    return signing_input + "." + sig;
}

JwtClaims verify_jwt(const std::string& token,
                     EVP_PKEY* public_key,
                     const std::string& expected_iss,
                     const std::string& expected_aud) {
    using json = nlohmann::json;
    // Split into header.payload.signature
    auto d1 = token.find('.');
    if (d1 == std::string::npos) throw std::runtime_error("malformed JWT");
    auto d2 = token.find('.', d1 + 1);
    if (d2 == std::string::npos) throw std::runtime_error("malformed JWT");

    std::string signing_input = token.substr(0, d2);
    std::string sig_b64 = token.substr(d2 + 1);

    if (!verify_rs256(signing_input, sig_b64, public_key))
        throw std::runtime_error("JWT signature invalid");

    auto payload_bytes = b64url_decode(token.substr(d1 + 1, d2 - d1 - 1));
    std::string payload_str(payload_bytes.begin(), payload_bytes.end());
    json p = json::parse(payload_str);

    auto now = static_cast<int64_t>(std::time(nullptr));
    if (p.value("exp", int64_t{0}) <= now)
        throw std::runtime_error("JWT expired");
    if (p.value("iss", std::string{}) != expected_iss)
        throw std::runtime_error("JWT issuer mismatch");

    std::string aud = p.value("aud", std::string{});
    if (aud != expected_aud)
        throw std::runtime_error("JWT audience mismatch");

    JwtClaims c;
    c.iss       = p.value("iss",       std::string{});
    c.aud       = aud;
    c.sub       = p.value("sub",       std::string{});
    c.jti       = p.value("jti",       std::string{});
    c.scope     = p.value("scope",     std::string{});
    c.client_id = p.value("client_id", std::string{});
    c.iat       = p.value("iat",       int64_t{0});
    c.exp       = p.value("exp",       int64_t{0});
    return c;
}

// ── JWKS ──────────────────────────────────────────────────────────────────────

static std::string bignum_to_b64url(const BIGNUM* bn) {
    int len = BN_num_bytes(bn);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    BN_bn2bin(bn, buf.data());
    return b64url_encode(buf);
}

nlohmann::json public_key_to_jwk(EVP_PKEY* key, const std::string& kid) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    BIGNUM* n = nullptr; BIGNUM* e = nullptr;
    EVP_PKEY_get_bn_param(key, "n", &n);
    EVP_PKEY_get_bn_param(key, "e", &e);
#else
    const RSA* rsa = EVP_PKEY_get0_RSA(key);
    const BIGNUM *n_ref, *e_ref;
    RSA_get0_key(rsa, &n_ref, &e_ref, nullptr);
    BIGNUM* n = BN_dup(n_ref);
    BIGNUM* e = BN_dup(e_ref);
#endif
    auto n_str = bignum_to_b64url(n);
    auto e_str = bignum_to_b64url(e);
    BN_free(n); BN_free(e);
    return {{"kty","RSA"},{"use","sig"},{"alg","RS256"},{"kid",kid},
            {"n",n_str},{"e",e_str}};
}

// ── PKCE S256 ─────────────────────────────────────────────────────────────────

std::string pkce_challenge_s256(const std::string& verifier) {
    return b64url_encode(sha256(verifier));
}

bool pkce_verify(const std::string& verifier,
                 const std::string& challenge,
                 const std::string& method) {
    if (method != "S256") return false;
    auto expected = pkce_challenge_s256(verifier);
    // Constant-time comparison
    if (expected.size() != challenge.size()) return false;
    volatile int diff = 0;
    for (size_t i = 0; i < expected.size(); ++i)
        diff |= expected[i] ^ challenge[i];
    return diff == 0;
}

} // namespace crypto
