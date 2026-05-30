#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace agent {

struct MemoryEntry {
    std::string    id;
    std::string    content;
    nlohmann::json metadata;
    float          score{0.0f};
};

// Abstract memory backend. Implement for vector DB, SQLite, etc.
// The no-op default compiles cleanly and lets the rest of the system run.
class MemoryBackend {
public:
    virtual ~MemoryBackend() = default;

    virtual void write(const std::string& id, const std::string& content,
                       const nlohmann::json& metadata = {}) = 0;
    virtual std::vector<MemoryEntry> search(const std::string& query, int top_k = 5) = 0;
    virtual std::vector<MemoryEntry> list(const std::string& filter = "") = 0;
    virtual void remove(const std::string& id) = 0;
};

class NoOpMemoryBackend : public MemoryBackend {
public:
    void write(const std::string&, const std::string&, const nlohmann::json&) override {}
    std::vector<MemoryEntry> search(const std::string&, int) override { return {}; }
    std::vector<MemoryEntry> list(const std::string&) override { return {}; }
    void remove(const std::string&) override {}
};

} // namespace agent
