#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class AIModel {
public:
    virtual ~AIModel() = default;

    std::string Generate(const std::string& prompt,
                         float temperature = 0.7f, int max_tokens = 512);
    std::vector<float> Embed(const std::string& text, bool use_cache = true);
    float Similarity(const std::string& a, const std::string& b);
    std::vector<std::pair<float, std::string>> Search(
        const std::string& query,
        const std::vector<std::string>& labels,
        const std::vector<std::string>& texts,
        int top_n = 3);

    void ClearEmbedCache() noexcept { m_embedding_cache.clear(); }

    virtual std::string GetModelName()        const = 0;
    virtual int         GetMaxContextLength() const = 0;

    AIModel(const AIModel&)            = delete;
    AIModel& operator=(const AIModel&) = delete;
    AIModel(AIModel&&)                 = delete;
    AIModel& operator=(AIModel&&)      = delete;

protected:
    AIModel() = default;
    virtual std::string        RawGenerate(const std::string& prompt, float t, int max) = 0;
    virtual std::vector<float> RawEmbed(const std::string& text) = 0;
    std::unordered_map<std::string, std::vector<float>> m_embedding_cache;
};
