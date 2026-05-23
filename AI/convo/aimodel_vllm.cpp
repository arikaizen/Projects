#include "aimodel_vllm.hpp"
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

AIModelVLLM::AIModelVLLM(const std::string& base_url,
                         const std::string& model_name,
                         const std::string& api_key,
                         const std::string& embed_model_name,
                         int timeout_seconds,
                         int max_context)
    : m_base_url(base_url)
    , m_model_name(model_name)
    , m_embed_model_name(embed_model_name)
    , m_api_key(api_key)
    , m_max_context(max_context)
    , m_client(std::make_unique<httplib::Client>(base_url))
{
    m_client->set_connection_timeout(timeout_seconds);
    m_client->set_read_timeout(timeout_seconds);
    m_client->set_write_timeout(timeout_seconds);
}

std::string AIModelVLLM::GetModelName() const {
    return m_model_name;
}

int AIModelVLLM::GetMaxContextLength() const {
    return m_max_context;
}

httplib::Headers AIModelVLLM::AuthHeaders() const {
    if (!m_api_key.empty())
        return {{"Authorization", "Bearer " + m_api_key}};
    return {};
}

std::string AIModelVLLM::RawGenerate(const std::string& prompt, float t, int max) {
    json body = {
        {"model",       m_model_name},
        {"prompt",      prompt},
        {"temperature", t},
        {"max_tokens",  max}
    };

    auto res = m_client->Post("/v1/completions", AuthHeaders(),
                              body.dump(), "application/json");
    if (!res)
        throw std::runtime_error("AIModelVLLM::RawGenerate: HTTP request failed (no response)");
    if (res->status != 200)
        throw std::runtime_error(
            "AIModelVLLM::RawGenerate: HTTP " + std::to_string(res->status)
            + " — " + res->body);

    try {
        json result = json::parse(res->body);
        return result["choices"][0]["text"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelVLLM::RawGenerate: malformed response JSON — ") + e.what());
    }
}

std::vector<float> AIModelVLLM::RawEmbed(const std::string& text) {
    const std::string embed_model =
        m_embed_model_name.empty() ? m_model_name : m_embed_model_name;

    json body = {
        {"model", embed_model},
        {"input", text}
    };

    auto res = m_client->Post("/v1/embeddings", AuthHeaders(),
                              body.dump(), "application/json");
    if (!res)
        throw std::runtime_error("AIModelVLLM::RawEmbed: HTTP request failed (no response)");
    if (res->status != 200)
        throw std::runtime_error(
            "AIModelVLLM::RawEmbed: HTTP " + std::to_string(res->status)
            + " — " + res->body);

    try {
        json result = json::parse(res->body);
        return result["data"][0]["embedding"].get<std::vector<float>>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelVLLM::RawEmbed: malformed response JSON — ") + e.what());
    }
}

std::string AIModelVLLM::ChatCompletion(const nlohmann::json& messages,
                                        float temperature, int max_tokens) {
    json body = {
        {"model",       m_model_name},
        {"messages",    messages},
        {"temperature", temperature},
        {"max_tokens",  max_tokens}
    };

    auto res = m_client->Post("/v1/chat/completions", AuthHeaders(),
                              body.dump(), "application/json");
    if (!res)
        throw std::runtime_error("AIModelVLLM::ChatCompletion: HTTP request failed (no response)");
    if (res->status != 200)
        throw std::runtime_error(
            "AIModelVLLM::ChatCompletion: HTTP " + std::to_string(res->status)
            + " — " + res->body);

    try {
        json result = json::parse(res->body);
        return result["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AIModelVLLM::ChatCompletion: malformed response JSON — ") + e.what());
    }
}
