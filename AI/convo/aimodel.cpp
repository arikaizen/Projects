#include "aimodel.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace {
bool IsBlank(const std::string& s) {
    for (unsigned char c : s) if (!std::isspace(c)) return false;
    return true;
}
float VecNorm(const std::vector<float>& v) {
    return std::sqrt(std::inner_product(v.begin(), v.end(), v.begin(), 0.0f));
}
float CosineSim(const std::vector<float>& a, const std::vector<float>& b) {
    assert(a.size() == b.size());
    float dot = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
    float na = VecNorm(a), nb = VecNorm(b);
    if (na == 0.0f || nb == 0.0f) return 0.0f;
    return dot / (na * nb);
}
} // namespace

std::string AIModel::Generate(const std::string& prompt, float temperature, int max_tokens) {
    if (IsBlank(prompt))
        throw std::invalid_argument("AIModel::Generate: prompt must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument("AIModel::Generate: temperature must be in [0.0, 2.0]; got " + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument("AIModel::Generate: max_tokens must be >= 1; got " + std::to_string(max_tokens));
    auto result = RawGenerate(prompt, temperature, max_tokens);
    if (IsBlank(result))
        throw std::runtime_error("AIModel::Generate: model returned an empty response");
    return result;
}

std::vector<float> AIModel::Embed(const std::string& text, bool use_cache) {
    if (IsBlank(text))
        throw std::invalid_argument("AIModel::Embed: text must not be blank");
    if (use_cache) {
        auto it = m_embedding_cache.find(text);
        if (it != m_embedding_cache.end()) return it->second;
    }
    auto result = RawEmbed(text);
    if (VecNorm(result) == 0.0f)
        throw std::runtime_error("AIModel::Embed: embedding has zero magnitude (does this model support embeddings?)");
    if (use_cache) m_embedding_cache[text] = result;
    return result;
}

float AIModel::Similarity(const std::string& a, const std::string& b) {
    if (IsBlank(a)) throw std::invalid_argument("AIModel::Similarity: first text must not be blank");
    if (IsBlank(b)) throw std::invalid_argument("AIModel::Similarity: second text must not be blank");
    return CosineSim(Embed(a), Embed(b));
}

std::vector<std::pair<float, std::string>>
AIModel::Search(const std::string& query, const std::vector<std::string>& labels,
                const std::vector<std::string>& texts, int top_n) {
    if (IsBlank(query))
        throw std::invalid_argument("AIModel::Search: query must not be blank");
    if (labels.size() != texts.size())
        throw std::invalid_argument("AIModel::Search: labels and texts must have the same length; got "
            + std::to_string(labels.size()) + " vs " + std::to_string(texts.size()));
    if (top_n < 1)
        throw std::invalid_argument("AIModel::Search: top_n must be >= 1; got " + std::to_string(top_n));
    auto qemb = Embed(query);
    std::vector<std::pair<float, std::string>> ranked;
    ranked.reserve(texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i)
        ranked.emplace_back(CosineSim(qemb, Embed(texts[i])), labels[i]);
    std::sort(ranked.begin(), ranked.end(), [](const auto& x, const auto& y){ return x.first > y.first; });
    int n = std::min(top_n, static_cast<int>(ranked.size()));
    return {ranked.begin(), ranked.begin() + n};
}
