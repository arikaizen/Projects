#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Structured per-agent logger.
//
// Every run appends to <log_dir>/<agent_id>.jsonl — one flat JSON object per
// line — so the full execution trace can be queried with standard JSON tools.
// The same events are also printed to stderr in a human-readable format.
//
// All public methods are thread-safe.
class AgentLogger {
public:
    explicit AgentLogger(std::filesystem::path log_dir);

    // ── Agent lifecycle ──────────────────────────────────────────────────────
    void agentStart(const std::string& agent_id, const std::string& task);
    void agentDone (const std::string& agent_id, const std::string& reason,
                    const nlohmann::json& output, int iterations);

    // ── Batch dispatch ───────────────────────────────────────────────────────
    void batchStart(const std::string& agent_id, int size,
                    const std::string& phase,
                    const std::vector<std::string>& item_ids);

    // ── Stage lifecycle ──────────────────────────────────────────────────────
    void stageStart(const std::string& agent_id, const std::string& stage_name,
                    const std::string& stage_id, const nlohmann::json& inputs);
    void stageDone (const std::string& agent_id, const std::string& stage_name,
                    const std::string& stage_id, bool success,
                    const nlohmann::json& output, long duration_ms,
                    const std::string& error = {});

    // ── LLM calls ────────────────────────────────────────────────────────────
    void llmCall    (const std::string& agent_id, const std::string& stage_name,
                     const std::string& stage_id, const std::string& system_prompt,
                     const std::string& user_msg,  bool json_mode,
                     float temperature, int max_tokens);
    void llmResponse(const std::string& agent_id, const std::string& stage_name,
                     const std::string& stage_id, const std::string& content,
                     bool success, const std::string& error = {});

    // ── Plan events ───────────────────────────────────────────────────────────
    void planPushed(const std::string& agent_id, const std::string& stage_name,
                    const std::string& stage_id, const nlohmann::json& plan);

    // ── Action lifecycle ─────────────────────────────────────────────────────
    void actionStart(const std::string& agent_id, const std::string& action_name,
                     const std::string& action_id, const nlohmann::json& resolved_inputs);
    void actionDone (const std::string& agent_id, const std::string& action_name,
                     const std::string& action_id, bool success,
                     const nlohmann::json& output, long duration_ms,
                     const std::string& error = {});

    // ── Semantic events ──────────────────────────────────────────────────────
    void blackboardWrite  (const std::string& agent_id, const std::string& stage_name,
                           const std::string& key, const nlohmann::json& value);
    void observeDecision  (const std::string& agent_id, const std::string& stage_id,
                           bool done, const std::string& next_action,
                           const nlohmann::json& observations,
                           const nlohmann::json& failures);
    void finalAnswer      (const std::string& agent_id, const nlohmann::json& answer,
                           int iteration);
    void cacheEvent       (const std::string& agent_id, const std::string& sub_event,
                           const nlohmann::json& data = {});

private:
    // Opens (or returns existing) file stream for agent_id.  Caller holds m_mutex.
    std::ofstream& openStream(const std::string& agent_id);

    // Write one JSON line to both the file and stderr.
    void writeEntry(const std::string& agent_id, nlohmann::json entry);

    // Format a short human-readable prefix: "[HH:MM:SS.mmm] [agent_id]"
    static std::string prefix(const std::string& agent_id);
    static std::string nowISO();   // full ISO-8601 with milliseconds (for JSONL)
    static std::string nowHMS();   // HH:MM:SS.mmm  (for stderr)

    // Truncate a string to max_len chars for stderr, appending "…" if cut.
    static std::string trunc(const std::string& s, size_t max_len = 300);

    // Print multi-line content as an indented block to stderr.
    void printBlock(const std::string& agent_id, const std::string& header,
                    const std::string& content, const std::string& indent = "    ");

    std::filesystem::path                m_log_dir;
    mutable std::mutex                   m_mutex;
    std::map<std::string, std::ofstream> m_streams;
};

} // namespace agent
