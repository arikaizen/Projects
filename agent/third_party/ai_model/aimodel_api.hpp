#pragma once
#include "aimodel.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

// API-backed AIModel implementations.
//
// Each class wraps one remote LLM provider protocol behind the AIModel
// interface so engine agents can run on hosted models (ChatGPT, Claude,
// Gemini) or local servers (Ollama, LM Studio, llama.cpp, vLLM) without any
// other engine change.
//
// All classes accept a full base URL ("https://api.openai.com",
// "http://localhost:11434", "https://api.groq.com/openai"). A path component
// in the URL is honoured as a prefix on every request, so OpenAI-compatible
// vendors that nest their API (Groq, OpenRouter) work out of the box.
//
// Credentials are read once at construction and attached to every request by
// this layer; they are never echoed into prompts or logs.

// Splits "scheme://host[:port][/prefix]" into the client origin and the path
// prefix httplib needs them in.
struct ApiEndpoint {
    std::string origin;       // "https://api.groq.com"
    std::string path_prefix;  // "/openai" (may be empty)
    static ApiEndpoint parse(const std::string& base_url);
};

// ── OpenAI Chat Completions protocol ─────────────────────────────────────────
// Covers: OpenAI/ChatGPT, Groq, Mistral, DeepSeek, xAI/Grok, OpenRouter,
// Together, LM Studio, llama.cpp server, vLLM, and any other
// OpenAI-compatible endpoint.
class AIModelOpenAI final : public AIModel {
public:
    AIModelOpenAI(const std::string& base_url,
                  const std::string& model_name,
                  const std::string& api_key = "",
                  const std::string& embed_model_name = "",
                  int timeout_seconds = 120,
                  int max_context = 128000);

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    std::string ChatCompletion(const nlohmann::json& messages,
                               float temperature, int max_tokens);

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;

private:
    httplib::Headers authHeaders() const;

    std::string                      m_model_name;
    std::string                      m_embed_model_name;
    std::string                      m_api_key;
    std::string                      m_prefix;
    int                              m_max_context;
    std::unique_ptr<httplib::Client> m_client;
};

// ── Anthropic Messages protocol (Claude) ─────────────────────────────────────
class AIModelAnthropic final : public AIModel {
public:
    AIModelAnthropic(const std::string& base_url,
                     const std::string& model_name,
                     const std::string& api_key,
                     int timeout_seconds = 120,
                     int max_context = 200000);

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    // messages follows the Anthropic schema; system may be empty.
    std::string ChatCompletion(const nlohmann::json& messages,
                               const std::string& system,
                               float temperature, int max_tokens);

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;  // unsupported → throws

private:
    std::string                      m_model_name;
    std::string                      m_api_key;
    std::string                      m_prefix;
    int                              m_max_context;
    std::unique_ptr<httplib::Client> m_client;
};

// ── Google Generative Language protocol (Gemini) ─────────────────────────────
// Auth: API key (?key=...) by default; pass auth_bearer=true to send an OAuth
// access token as an Authorization: Bearer header instead (Google sign-in).
class AIModelGemini final : public AIModel {
public:
    AIModelGemini(const std::string& base_url,
                  const std::string& model_name,
                  const std::string& credential,
                  bool auth_bearer = false,
                  const std::string& embed_model_name = "",
                  int timeout_seconds = 120,
                  int max_context = 1000000);

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    std::string ChatCompletion(const nlohmann::json& contents,
                               const std::string& system,
                               float temperature, int max_tokens);

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;

private:
    std::string      pathFor(const std::string& model, const std::string& verb) const;
    httplib::Headers authHeaders() const;

    std::string                      m_model_name;
    std::string                      m_embed_model_name;
    std::string                      m_credential;
    bool                             m_auth_bearer;
    std::string                      m_prefix;
    int                              m_max_context;
    std::unique_ptr<httplib::Client> m_client;
};

// ── Ollama native protocol (local models) ────────────────────────────────────
class AIModelOllama final : public AIModel {
public:
    AIModelOllama(const std::string& base_url,
                  const std::string& model_name,
                  const std::string& embed_model_name = "",
                  int timeout_seconds = 300,
                  int max_context = 8192);

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    std::string ChatCompletion(const nlohmann::json& messages,
                               float temperature, int max_tokens);

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;

private:
    std::string                      m_model_name;
    std::string                      m_embed_model_name;
    std::string                      m_prefix;
    int                              m_max_context;
    std::unique_ptr<httplib::Client> m_client;
};

// ── Provider factory ──────────────────────────────────────────────────────────
// Builds the right AIModel from a JSON config:
//   {
//     "provider":    "openai" | "anthropic" | "google" | "ollama" | "groq" |
//                    "mistral" | "deepseek" | "xai" | "openrouter" |
//                    "together" | "lmstudio" | "llamacpp" | "vllm" | "custom",
//     "model":       "gpt-4o" / "claude-sonnet-4-6" / "gemini-2.0-flash" / ...,
//     "api_key":     "...",            // or OAuth access token
//     "auth_method": "api_key" | "bearer",   // google only; default api_key
//     "base_url":    "https://...",    // optional — provider default used
//     "embed_model": "...",            // optional
//     "timeout_seconds": 120,          // optional
//     "max_context": 128000            // optional
//   }
// Unknown providers fall back to the OpenAI-compatible protocol so any future
// vendor with a /v1/chat/completions endpoint works via provider:"custom".
// Throws std::runtime_error on invalid config (e.g. missing model).
std::unique_ptr<AIModel> createApiModel(const nlohmann::json& cfg);

// Default base URL for a provider id ("" when unknown).
std::string apiProviderDefaultBaseUrl(const std::string& provider);
