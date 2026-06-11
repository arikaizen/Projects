#include "aimodel_api.hpp"
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

// ── URL splitting ─────────────────────────────────────────────────────────────

ApiEndpoint ApiEndpoint::parse(const std::string& base_url) {
    auto scheme_end = base_url.find("://");
    if (scheme_end == std::string::npos)
        throw std::runtime_error("ApiEndpoint: malformed base URL (no scheme): " + base_url);

    auto path_start = base_url.find('/', scheme_end + 3);
    ApiEndpoint ep;
    if (path_start == std::string::npos) {
        ep.origin = base_url;
    } else {
        ep.origin      = base_url.substr(0, path_start);
        ep.path_prefix = base_url.substr(path_start);
        // Normalise: no trailing slash on the prefix
        while (!ep.path_prefix.empty() && ep.path_prefix.back() == '/')
            ep.path_prefix.pop_back();
    }
    return ep;
}

static std::unique_ptr<httplib::Client> makeClient(const std::string& origin,
                                                   int timeout_seconds) {
    auto cli = std::make_unique<httplib::Client>(origin);
    cli->set_connection_timeout(timeout_seconds);
    cli->set_read_timeout(timeout_seconds);
    cli->set_write_timeout(timeout_seconds);
    cli->set_follow_location(true);
    return cli;
}

static std::string postJson(httplib::Client& cli,
                            const std::string& path,
                            const httplib::Headers& headers,
                            const json& body,
                            const char* who) {
    auto res = cli.Post(path.c_str(), headers, body.dump(), "application/json");
    if (!res)
        throw std::runtime_error(std::string(who) + ": HTTP request failed: " +
                                 httplib::to_string(res.error()));
    if (res->status < 200 || res->status >= 300)
        throw std::runtime_error(std::string(who) + ": HTTP " +
                                 std::to_string(res->status) + " — " + res->body);
    return res->body;
}

// ── AIModelOpenAI ─────────────────────────────────────────────────────────────

AIModelOpenAI::AIModelOpenAI(const std::string& base_url,
                             const std::string& model_name,
                             const std::string& api_key,
                             const std::string& embed_model_name,
                             int timeout_seconds,
                             int max_context)
    : m_model_name(model_name)
    , m_embed_model_name(embed_model_name)
    , m_api_key(api_key)
    , m_max_context(max_context)
{
    auto ep  = ApiEndpoint::parse(base_url);
    m_prefix = ep.path_prefix;
    m_client = makeClient(ep.origin, timeout_seconds);
}

std::string AIModelOpenAI::GetModelName() const        { return m_model_name; }
int         AIModelOpenAI::GetMaxContextLength() const { return m_max_context; }

httplib::Headers AIModelOpenAI::authHeaders() const {
    if (!m_api_key.empty())
        return {{"Authorization", "Bearer " + m_api_key}};
    return {};
}

std::string AIModelOpenAI::ChatCompletion(const json& messages,
                                          float temperature, int max_tokens) {
    json body = {
        {"model",       m_model_name},
        {"messages",    messages},
        {"temperature", temperature},
        {"max_tokens",  max_tokens},
    };
    auto raw = postJson(*m_client, m_prefix + "/v1/chat/completions",
                        authHeaders(), body, "AIModelOpenAI::ChatCompletion");
    try {
        json r = json::parse(raw);
        return r["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelOpenAI::ChatCompletion: malformed response JSON — ") + e.what());
    }
}

std::string AIModelOpenAI::RawGenerate(const std::string& prompt, float t, int max) {
    return ChatCompletion(json::array({{{"role", "user"}, {"content", prompt}}}), t, max);
}

std::vector<float> AIModelOpenAI::RawEmbed(const std::string& text) {
    const std::string embed_model =
        m_embed_model_name.empty() ? "text-embedding-3-small" : m_embed_model_name;
    json body = {{"model", embed_model}, {"input", text}};
    auto raw = postJson(*m_client, m_prefix + "/v1/embeddings",
                        authHeaders(), body, "AIModelOpenAI::RawEmbed");
    try {
        json r = json::parse(raw);
        return r["data"][0]["embedding"].get<std::vector<float>>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelOpenAI::RawEmbed: malformed response JSON — ") + e.what());
    }
}

// ── AIModelAnthropic ──────────────────────────────────────────────────────────

AIModelAnthropic::AIModelAnthropic(const std::string& base_url,
                                   const std::string& model_name,
                                   const std::string& api_key,
                                   int timeout_seconds,
                                   int max_context)
    : m_model_name(model_name)
    , m_api_key(api_key)
    , m_max_context(max_context)
{
    auto ep  = ApiEndpoint::parse(base_url);
    m_prefix = ep.path_prefix;
    m_client = makeClient(ep.origin, timeout_seconds);
}

std::string AIModelAnthropic::GetModelName() const        { return m_model_name; }
int         AIModelAnthropic::GetMaxContextLength() const { return m_max_context; }

std::string AIModelAnthropic::ChatCompletion(const json& messages,
                                             const std::string& system,
                                             float temperature, int max_tokens) {
    json body = {
        {"model",       m_model_name},
        {"messages",    messages},
        {"temperature", temperature},
        {"max_tokens",  max_tokens},
    };
    if (!system.empty()) body["system"] = system;

    httplib::Headers headers = {
        {"x-api-key",         m_api_key},
        {"anthropic-version", "2023-06-01"},
    };
    auto raw = postJson(*m_client, m_prefix + "/v1/messages",
                        headers, body, "AIModelAnthropic::ChatCompletion");
    try {
        json r = json::parse(raw);
        // content is an array of blocks; concatenate the text blocks.
        std::string out;
        for (const auto& block : r["content"]) {
            if (block.value("type", "") == "text")
                out += block.value("text", "");
        }
        return out;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelAnthropic::ChatCompletion: malformed response JSON — ") + e.what());
    }
}

std::string AIModelAnthropic::RawGenerate(const std::string& prompt, float t, int max) {
    return ChatCompletion(json::array({{{"role", "user"}, {"content", prompt}}}),
                          /*system=*/"", t, max);
}

std::vector<float> AIModelAnthropic::RawEmbed(const std::string&) {
    throw std::runtime_error(
        "AIModelAnthropic::RawEmbed: the Anthropic API has no embeddings endpoint; "
        "configure a separate embed provider");
}

// ── AIModelGemini ─────────────────────────────────────────────────────────────

AIModelGemini::AIModelGemini(const std::string& base_url,
                             const std::string& model_name,
                             const std::string& credential,
                             bool auth_bearer,
                             const std::string& embed_model_name,
                             int timeout_seconds,
                             int max_context)
    : m_model_name(model_name)
    , m_embed_model_name(embed_model_name)
    , m_credential(credential)
    , m_auth_bearer(auth_bearer)
    , m_max_context(max_context)
{
    auto ep  = ApiEndpoint::parse(base_url);
    m_prefix = ep.path_prefix;
    m_client = makeClient(ep.origin, timeout_seconds);
}

std::string AIModelGemini::GetModelName() const        { return m_model_name; }
int         AIModelGemini::GetMaxContextLength() const { return m_max_context; }

std::string AIModelGemini::pathFor(const std::string& model,
                                   const std::string& verb) const {
    std::string path = m_prefix + "/v1beta/models/" + model + ":" + verb;
    if (!m_auth_bearer && !m_credential.empty())
        path += "?key=" + m_credential;
    return path;
}

httplib::Headers AIModelGemini::authHeaders() const {
    if (m_auth_bearer && !m_credential.empty())
        return {{"Authorization", "Bearer " + m_credential}};
    return {};
}

std::string AIModelGemini::ChatCompletion(const json& contents,
                                          const std::string& system,
                                          float temperature, int max_tokens) {
    json body = {
        {"contents", contents},
        {"generationConfig", {
            {"temperature",     temperature},
            {"maxOutputTokens", max_tokens},
        }},
    };
    if (!system.empty())
        body["systemInstruction"] = {{"parts", json::array({{{"text", system}}})}};

    auto raw = postJson(*m_client, pathFor(m_model_name, "generateContent"),
                        authHeaders(), body, "AIModelGemini::ChatCompletion");
    try {
        json r = json::parse(raw);
        std::string out;
        for (const auto& part : r["candidates"][0]["content"]["parts"]) {
            out += part.value("text", "");
        }
        return out;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelGemini::ChatCompletion: malformed response JSON — ") + e.what());
    }
}

std::string AIModelGemini::RawGenerate(const std::string& prompt, float t, int max) {
    json contents = json::array({
        {{"role", "user"}, {"parts", json::array({{{"text", prompt}}})}}
    });
    return ChatCompletion(contents, /*system=*/"", t, max);
}

std::vector<float> AIModelGemini::RawEmbed(const std::string& text) {
    const std::string embed_model =
        m_embed_model_name.empty() ? "text-embedding-004" : m_embed_model_name;
    json body = {
        {"content", {{"parts", json::array({{{"text", text}}})}}},
    };
    auto raw = postJson(*m_client, pathFor(embed_model, "embedContent"),
                        authHeaders(), body, "AIModelGemini::RawEmbed");
    try {
        json r = json::parse(raw);
        return r["embedding"]["values"].get<std::vector<float>>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelGemini::RawEmbed: malformed response JSON — ") + e.what());
    }
}

// ── AIModelOllama ─────────────────────────────────────────────────────────────

AIModelOllama::AIModelOllama(const std::string& base_url,
                             const std::string& model_name,
                             const std::string& embed_model_name,
                             int timeout_seconds,
                             int max_context)
    : m_model_name(model_name)
    , m_embed_model_name(embed_model_name)
    , m_max_context(max_context)
{
    auto ep  = ApiEndpoint::parse(base_url);
    m_prefix = ep.path_prefix;
    m_client = makeClient(ep.origin, timeout_seconds);
}

std::string AIModelOllama::GetModelName() const        { return m_model_name; }
int         AIModelOllama::GetMaxContextLength() const { return m_max_context; }

std::string AIModelOllama::ChatCompletion(const json& messages,
                                          float temperature, int max_tokens) {
    json body = {
        {"model",    m_model_name},
        {"messages", messages},
        {"stream",   false},
        {"options",  {
            {"temperature", temperature},
            {"num_predict", max_tokens},
        }},
    };
    auto raw = postJson(*m_client, m_prefix + "/api/chat",
                        {}, body, "AIModelOllama::ChatCompletion");
    try {
        json r = json::parse(raw);
        return r["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelOllama::ChatCompletion: malformed response JSON — ") + e.what());
    }
}

std::string AIModelOllama::RawGenerate(const std::string& prompt, float t, int max) {
    return ChatCompletion(json::array({{{"role", "user"}, {"content", prompt}}}), t, max);
}

std::vector<float> AIModelOllama::RawEmbed(const std::string& text) {
    const std::string embed_model =
        m_embed_model_name.empty() ? m_model_name : m_embed_model_name;
    json body = {{"model", embed_model}, {"prompt", text}};
    auto raw = postJson(*m_client, m_prefix + "/api/embeddings",
                        {}, body, "AIModelOllama::RawEmbed");
    try {
        json r = json::parse(raw);
        return r["embedding"].get<std::vector<float>>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelOllama::RawEmbed: malformed response JSON — ") + e.what());
    }
}

// ── Factory ───────────────────────────────────────────────────────────────────

std::string apiProviderDefaultBaseUrl(const std::string& provider) {
    if (provider == "openai" || provider == "chatgpt") return "https://api.openai.com";
    if (provider == "anthropic" || provider == "claude") return "https://api.anthropic.com";
    if (provider == "google" || provider == "gemini") return "https://generativelanguage.googleapis.com";
    if (provider == "ollama")     return "http://localhost:11434";
    if (provider == "groq")       return "https://api.groq.com/openai";
    if (provider == "mistral")    return "https://api.mistral.ai";
    if (provider == "deepseek")   return "https://api.deepseek.com";
    if (provider == "xai" || provider == "grok") return "https://api.x.ai";
    if (provider == "openrouter") return "https://openrouter.ai/api";
    if (provider == "together")   return "https://api.together.xyz";
    if (provider == "lmstudio")   return "http://localhost:1234";
    if (provider == "llamacpp" || provider == "llama.cpp") return "http://localhost:8080";
    if (provider == "vllm")       return "http://localhost:8000";
    return "";
}

std::unique_ptr<AIModel> createApiModel(const json& cfg) {
    std::string provider = cfg.value("provider", cfg.value("backend", std::string{}));
    std::transform(provider.begin(), provider.end(), provider.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const std::string model = cfg.value("model", std::string{});
    if (model.empty())
        throw std::runtime_error("createApiModel: config requires a \"model\" field");

    std::string base_url = cfg.value("base_url", std::string{});
    if (base_url.empty()) base_url = apiProviderDefaultBaseUrl(provider);
    if (base_url.empty())
        throw std::runtime_error("createApiModel: unknown provider \"" + provider +
                                 "\" and no base_url supplied");

    const std::string api_key     = cfg.value("api_key", std::string{});
    const std::string embed_model = cfg.value("embed_model", std::string{});
    const int timeout             = cfg.value("timeout_seconds", 120);

    if (provider == "anthropic" || provider == "claude") {
        return std::make_unique<AIModelAnthropic>(
            base_url, model, api_key, timeout,
            cfg.value("max_context", 200000));
    }
    if (provider == "google" || provider == "gemini") {
        const bool bearer = cfg.value("auth_method", std::string{"api_key"}) == "bearer";
        return std::make_unique<AIModelGemini>(
            base_url, model, api_key, bearer, embed_model, timeout,
            cfg.value("max_context", 1000000));
    }
    if (provider == "ollama") {
        return std::make_unique<AIModelOllama>(
            base_url, model, embed_model,
            cfg.value("timeout_seconds", 300),
            cfg.value("max_context", 8192));
    }
    // Everything else speaks the OpenAI Chat Completions protocol.
    return std::make_unique<AIModelOpenAI>(
        base_url, model, api_key, embed_model, timeout,
        cfg.value("max_context", 128000));
}
