#pragma once
#include "agent/memory_backend.hpp"
#include "ai_model/aimodel.hpp"
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace agent {

// Adapter: backs the engine's MemoryBackend with an AIModel's embedding +
// semantic-search capability (AIModel::Embed / AIModel::Search).
//
// Entries are held in memory (label + content + metadata); search() delegates
// ranking to AIModel::Search, which embeds the query and every stored text and
// returns the top-k by cosine similarity.  The AIModel's own embedding cache
// keeps repeated embeds cheap.
//
// Thread-safety: the internal store is guarded by a mutex so concurrent agents
// may read/write memory safely.  AIModel::Embed is assumed thread-safe per the
// AIModel contract (its cache is the only shared state).
//
// The adapter holds a reference to an externally-owned AIModel; the model must
// outlive the adapter.
class AIModelMemoryBackend : public MemoryBackend {
public:
    explicit AIModelMemoryBackend(AIModel& model) : m_model(model) {}

    void write(const std::string& id, const std::string& content,
               const nlohmann::json& metadata = {}) override;
    std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) override;
    std::vector<MemoryEntry> list(const std::string& filter = "") override;
    void remove(const std::string& id) override;

private:
    struct Record {
        std::string    content;
        nlohmann::json metadata;
    };

    AIModel&                        m_model;
    mutable std::mutex              m_mutex;
    std::map<std::string, Record>   m_store;   // id -> record
};

} // namespace agent
