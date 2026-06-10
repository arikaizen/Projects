#pragma once
#include "agent/llm_client.hpp"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Builds an LLMClient from a JSON config object. Single entry point used by
// am_create's "llm" key, am_configure_llm, and per-agent "llm" overrides in
// spawn configs.
//
// Config shape:
//   {
//     "provider": "openai" | "anthropic" | "google" | "ollama" | "groq" |
//                 "mistral" | "deepseek" | "xai" | "openrouter" | "together" |
//                 "lmstudio" | "llamacpp" | "vllm" | "llama" | "custom" | "mock",
//     "model":    "...",
//     "api_key":  "...",
//     "base_url": "...",        // optional — provider default applies
//     ...                       // provider-specific extras (see aimodel_api.hpp)
//   }
//   "backend" is accepted as an alias for "provider" (older configs).
//
// Returns nullptr when the requested backend was not compiled into this build
// or the config is invalid; *error then carries the reason. Never throws.
std::shared_ptr<LLMClient> makeLLMClientFromConfig(const nlohmann::json& cfg,
                                                   std::string* error = nullptr);

} // namespace agent
