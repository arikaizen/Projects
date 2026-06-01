#pragma once
#include "aimodel.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

class AIModelVLLM final : public AIModel {
public:
    AIModelVLLM(const std::string& base_url,
                const std::string& model_name,
                const std::string& api_key = "",
                const std::string& embed_model_name = "",
                int timeout_seconds = 120,
                int max_context = 8192);

    std::string GetModelName()        const override;
    int         GetMaxContextLength() const override;

    std::string ChatCompletion(const nlohmann::json& messages,
                               float temperature, int max_tokens);
    httplib::Headers AuthHeaders() const;

protected:
    std::string        RawGenerate(const std::string& prompt, float t, int max) override;
    std::vector<float> RawEmbed(const std::string& text) override;

private:
    std::string                      m_base_url;
    std::string                      m_model_name;
    std::string                      m_embed_model_name;
    std::string                      m_api_key;
    int                              m_max_context;
    std::unique_ptr<httplib::Client> m_client;
};
