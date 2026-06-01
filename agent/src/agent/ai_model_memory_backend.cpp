#include "agent/ai_model_memory_backend.hpp"
#include <algorithm>

namespace agent {

void AIModelMemoryBackend::write(const std::string& id, const std::string& content,
                                 const nlohmann::json& metadata) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_store[id] = Record{ content, metadata };
}

std::vector<MemoryEntry> AIModelMemoryBackend::search(const std::string& query, int top_k) {
    // Snapshot the store under lock, then run the (potentially slow) embedding
    // search without holding the mutex.
    std::vector<std::string> labels;
    std::vector<std::string> texts;
    std::map<std::string, Record> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_store;
    }
    if (snapshot.empty() || query.empty()) return {};

    labels.reserve(snapshot.size());
    texts.reserve(snapshot.size());
    for (const auto& [id, rec] : snapshot) {
        labels.push_back(id);
        texts.push_back(rec.content);
    }

    auto ranked = m_model.Search(query, labels, texts, std::max(1, top_k));

    std::vector<MemoryEntry> out;
    out.reserve(ranked.size());
    for (const auto& [score, id] : ranked) {
        const auto& rec = snapshot.at(id);
        out.push_back(MemoryEntry{ id, rec.content, rec.metadata, score });
    }
    return out;
}

std::vector<MemoryEntry> AIModelMemoryBackend::list(const std::string& filter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MemoryEntry> out;
    for (const auto& [id, rec] : m_store) {
        if (filter.empty() ||
            id.find(filter) != std::string::npos ||
            rec.content.find(filter) != std::string::npos) {
            out.push_back(MemoryEntry{ id, rec.content, rec.metadata, 0.0f });
        }
    }
    return out;
}

void AIModelMemoryBackend::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_store.erase(id);
}

} // namespace agent
