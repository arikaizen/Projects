#include "agent/llm_factory.hpp"
#include "agent/ai_model_llm_client.hpp"

#ifdef AGENT_HAS_API_LLM
#include "ai_model/aimodel_api.hpp"
#endif
#ifdef AGENT_HAS_VLLM
#include "ai_model/aimodel_vllm.hpp"
#endif
#ifdef AGENT_HAS_LLAMA
#include "ai_model/aimodel_llama.hpp"
#endif

#include <algorithm>

namespace agent {

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::shared_ptr<LLMClient> makeLLMClientFromConfig(const nlohmann::json& cfg,
                                                   std::string* error) {
    auto fail = [&](const std::string& msg) -> std::shared_ptr<LLMClient> {
        if (error) *error = msg;
        return nullptr;
    };

    try {
        if (!cfg.is_object())
            return fail("llm config must be a JSON object");

        const std::string provider =
            lower(cfg.value("provider", cfg.value("backend", std::string{})));

        if (provider.empty())
            return fail("llm config requires a \"provider\" (or \"backend\") field");

        if (provider == "mock") {
            return std::make_shared<MockLLMClient>(
                [](const LLMClient::Request&) -> LLMClient::Response {
                    return {"[]", true, "", {}};
                });
        }

        if (provider == "llama" || provider == "llama_local") {
#ifdef AGENT_HAS_LLAMA
            const std::string path = cfg.value("model_path", cfg.value("model", std::string{}));
            if (path.empty())
                return fail("llama provider requires \"model_path\"");
            auto model = std::make_unique<AIModelLlama>(
                path,
                cfg.value("max_context", 4096),
                cfg.value("threads", 4));
            return std::make_shared<AIModelLLMClient>(std::move(model));
#else
            return fail("provider \"llama\" requires a build with -DAGENT_ENABLE_LLAMA=ON");
#endif
        }

        if (provider == "vllm") {
            // Prefer the generic API backend; keep the dedicated vLLM class as
            // fallback for builds that only enabled AGENT_ENABLE_VLLM.
#ifdef AGENT_HAS_API_LLM
            return std::make_shared<AIModelLLMClient>(createApiModel(cfg));
#elif defined(AGENT_HAS_VLLM)
            auto model = std::make_unique<AIModelVLLM>(
                cfg.value("base_url", std::string{"http://localhost:8000"}),
                cfg.value("model",       std::string{}),
                cfg.value("api_key",     std::string{}),
                cfg.value("embed_model", std::string{}),
                cfg.value("timeout_seconds", 120),
                cfg.value("max_context", 8192));
            return std::make_shared<AIModelLLMClient>(std::move(model));
#else
            return fail("provider \"vllm\" requires a build with "
                        "-DAGENT_ENABLE_API_LLM=ON or -DAGENT_ENABLE_VLLM=ON");
#endif
        }

#ifdef AGENT_HAS_API_LLM
        return std::make_shared<AIModelLLMClient>(createApiModel(cfg));
#else
        return fail("provider \"" + provider + "\" requires a build with "
                    "-DAGENT_ENABLE_API_LLM=ON (cpp-httplib + OpenSSL)");
#endif

    } catch (const std::exception& e) {
        return fail(std::string("makeLLMClientFromConfig: ") + e.what());
    } catch (...) {
        return fail("makeLLMClientFromConfig: unknown error");
    }
}

} // namespace agent
