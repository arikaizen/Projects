#include "agent/agent_logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace agent {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AgentLogger::AgentLogger(std::filesystem::path log_dir)
    : m_log_dir(std::move(log_dir))
{
    std::filesystem::create_directories(m_log_dir);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::ofstream& AgentLogger::openStream(const std::string& agent_id)
{
    auto it = m_streams.find(agent_id);
    if (it != m_streams.end()) return it->second;

    // Sanitise agent_id for use as a filename component.
    std::string safe = agent_id;
    for (char& c : safe)
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') c = '_';

    auto path = m_log_dir / (safe + ".jsonl");
    std::ofstream f(path, std::ios::app);
    if (!f.is_open())
        throw std::runtime_error("AgentLogger: cannot open log file " + path.string());
    m_streams[agent_id] = std::move(f);
    return m_streams[agent_id];
}

void AgentLogger::writeEntry(const std::string& agent_id, nlohmann::json entry)
{
    entry["ts"]    = nowISO();
    entry["agent"] = agent_id;

    std::unique_lock<std::mutex> lock(m_mutex);
    try {
        openStream(agent_id) << entry.dump() << "\n";
        openStream(agent_id).flush();
    } catch (...) {}
}

std::string AgentLogger::nowISO()
{
    using namespace std::chrono;
    auto tp  = system_clock::now();
    auto ms  = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

std::string AgentLogger::nowHMS()
{
    using namespace std::chrono;
    auto tp  = system_clock::now();
    auto ms  = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tm_loc{};
#if defined(_WIN32)
    localtime_s(&tm_loc, &t);
#else
    localtime_r(&t, &tm_loc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_loc, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string AgentLogger::prefix(const std::string& agent_id)
{
    return "[" + nowHMS() + "] [" + agent_id + "] ";
}

std::string AgentLogger::trunc(const std::string& s, size_t max_len)
{
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len) + "…";
}

// Print multi-line content with per-line prefix+indent, used for LLM blocks.
void AgentLogger::printBlock(const std::string& agent_id,
                              const std::string& header,
                              const std::string& content,
                              const std::string& indent)
{
    std::cerr << prefix(agent_id) << indent << header << "\n";
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line))
        std::cerr << prefix(agent_id) << indent << "│ " << line << "\n";
    std::cerr << prefix(agent_id) << indent << "└─\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Agent lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::agentStart(const std::string& agent_id, const std::string& task)
{
    std::cerr << prefix(agent_id) << "═══ AGENT START ═══\n";
    printBlock(agent_id, "task:", task, "  ");
    writeEntry(agent_id, {{"event","agent_start"},{"task",task}});
}

void AgentLogger::agentDone(const std::string& agent_id, const std::string& reason,
                              const nlohmann::json& output, int iterations)
{
    std::cerr << prefix(agent_id) << "═══ AGENT DONE: " << reason
              << " | iterations=" << iterations << " ═══\n";
    writeEntry(agent_id, {{"event","agent_done"},{"reason",reason},
                          {"output",output},{"iterations",iterations}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch dispatch
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::batchStart(const std::string& agent_id, int size,
                               const std::string& phase,
                               const std::vector<std::string>& item_ids)
{
    std::string ids;
    for (size_t i = 0; i < item_ids.size(); ++i) {
        if (i) ids += ", ";
        ids += item_ids[i];
    }
    std::cerr << prefix(agent_id) << "─── Batch (" << phase << "): "
              << size << " item(s) [" << ids << "]\n";
    writeEntry(agent_id, {{"event","batch_start"},{"phase",phase},
                          {"size",size},{"items",item_ids}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Stage lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::stageStart(const std::string& agent_id, const std::string& stage_name,
                               const std::string& stage_id, const nlohmann::json& inputs)
{
    std::cerr << prefix(agent_id) << "─── Stage: " << stage_name
              << " (" << stage_id << ")\n";
    if (!inputs.is_null() && !inputs.empty()) {
        std::string inp_str = inputs.dump(2);
        std::istringstream ss(inp_str);
        std::string line;
        std::cerr << prefix(agent_id) << "    inputs:\n";
        while (std::getline(ss, line))
            std::cerr << prefix(agent_id) << "      " << line << "\n";
    }
    writeEntry(agent_id, {{"event","stage_start"},{"stage",stage_name},
                          {"stage_id",stage_id},{"inputs",inputs}});
}

void AgentLogger::stageDone(const std::string& agent_id, const std::string& stage_name,
                              const std::string& stage_id, bool success,
                              const nlohmann::json& output, long duration_ms,
                              const std::string& error)
{
    std::string mark = success ? "✓" : "✗";
    std::cerr << prefix(agent_id) << "─── Stage DONE: " << stage_name
              << " " << mark << " (" << duration_ms << "ms)\n";
    if (success && !output.is_null() && !output.empty()) {
        std::string out_str = output.dump(2);
        std::istringstream ss(out_str);
        std::string line;
        std::cerr << prefix(agent_id) << "    output:\n";
        while (std::getline(ss, line))
            std::cerr << prefix(agent_id) << "      " << line << "\n";
    }
    if (!success && !error.empty())
        std::cerr << prefix(agent_id) << "    error: " << error << "\n";
    writeEntry(agent_id, {{"event","stage_done"},{"stage",stage_name},
                          {"stage_id",stage_id},{"success",success},
                          {"output",output},{"duration_ms",duration_ms},
                          {"error",error}});
}

// ─────────────────────────────────────────────────────────────────────────────
// LLM calls
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::llmCall(const std::string& agent_id, const std::string& stage_name,
                           const std::string& stage_id, const std::string& system_prompt,
                           const std::string& user_msg, bool json_mode,
                           float temperature, int max_tokens)
{
    std::cerr << prefix(agent_id) << "    ┌── LLM CALL [" << stage_name << "]"
              << " temp=" << temperature << " max_tokens=" << max_tokens
              << (json_mode ? " json_mode" : "") << "\n";
    printBlock(agent_id,
               "SYSTEM PROMPT (" + std::to_string(system_prompt.size()) + " chars):",
               system_prompt, "    ");
    printBlock(agent_id,
               "USER (" + std::to_string(user_msg.size()) + " chars):",
               user_msg, "    ");
    writeEntry(agent_id, {{"event","llm_call"},{"stage",stage_name},{"stage_id",stage_id},
                          {"system_prompt",system_prompt},{"user_msg",user_msg},
                          {"json_mode",json_mode},{"temperature",temperature},
                          {"max_tokens",max_tokens}});
}

void AgentLogger::llmResponse(const std::string& agent_id, const std::string& stage_name,
                               const std::string& stage_id, const std::string& content,
                               bool success, const std::string& error)
{
    if (success) {
        printBlock(agent_id,
                   "LLM RESPONSE ✓ (" + std::to_string(content.size()) + " chars):",
                   content, "    ");
    } else {
        std::cerr << prefix(agent_id) << "    LLM RESPONSE ✗: " << error << "\n";
    }
    writeEntry(agent_id, {{"event","llm_response"},{"stage",stage_name},{"stage_id",stage_id},
                          {"content",content},{"success",success},{"error",error}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Plan events
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::planPushed(const std::string& agent_id, const std::string& stage_name,
                              const std::string& stage_id, const nlohmann::json& plan)
{
    std::cerr << prefix(agent_id) << "    PLAN (" << plan.size() << " item(s)):\n";
    if (plan.is_array()) {
        for (size_t i = 0; i < plan.size(); ++i) {
            std::string iname = plan[i].value("name", std::string("?"));
            std::string iid   = plan[i].value("id",   std::string("?"));
            nlohmann::json inp = plan[i].value("inputs", nlohmann::json::object());
            std::cerr << prefix(agent_id) << "      [" << i << "] " << iname << " (id=" << iid << ")\n";
            if (!inp.empty()) {
                std::string inp_str = inp.dump(2);
                std::istringstream ss(inp_str);
                std::string line;
                while (std::getline(ss, line))
                    std::cerr << prefix(agent_id) << "           " << line << "\n";
            }
        }
    }
    writeEntry(agent_id, {{"event","plan_pushed"},{"stage",stage_name},
                          {"stage_id",stage_id},{"plan",plan}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Action lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::actionStart(const std::string& agent_id, const std::string& action_name,
                               const std::string& action_id, const nlohmann::json& resolved_inputs)
{
    std::cerr << prefix(agent_id) << "─── Action: " << action_name
              << " (" << action_id << ")\n";
    if (!resolved_inputs.is_null() && !resolved_inputs.empty()) {
        std::string inp_str = resolved_inputs.dump(2);
        std::istringstream ss(inp_str);
        std::string line;
        std::cerr << prefix(agent_id) << "    inputs:\n";
        while (std::getline(ss, line))
            std::cerr << prefix(agent_id) << "      " << line << "\n";
    }
    writeEntry(agent_id, {{"event","action_start"},{"action",action_name},
                          {"action_id",action_id},{"inputs",resolved_inputs}});
}

void AgentLogger::actionDone(const std::string& agent_id, const std::string& action_name,
                              const std::string& action_id, bool success,
                              const nlohmann::json& output, long duration_ms,
                              const std::string& error)
{
    std::string mark = success ? "✓" : "✗";
    std::cerr << prefix(agent_id) << "─── Action DONE: " << action_name
              << " " << mark << " (" << duration_ms << "ms)\n";
    if (success && !output.is_null() && !output.empty()) {
        std::string out_str = output.dump(2);
        std::istringstream ss(out_str);
        std::string line;
        std::cerr << prefix(agent_id) << "    output:\n";
        while (std::getline(ss, line))
            std::cerr << prefix(agent_id) << "      " << line << "\n";
    }
    if (!success)
        std::cerr << prefix(agent_id) << "    error: " << error << "\n";
    writeEntry(agent_id, {{"event","action_done"},{"action",action_name},
                          {"action_id",action_id},{"success",success},
                          {"output",output},{"duration_ms",duration_ms},{"error",error}});
}

// ─────────────────────────────────────────────────────────────────────────────
// Semantic events
// ─────────────────────────────────────────────────────────────────────────────

void AgentLogger::blackboardWrite(const std::string& agent_id, const std::string& stage_name,
                                   const std::string& key, const nlohmann::json& value)
{
    std::string val_str = value.is_string() ? value.get<std::string>() : value.dump(2);
    std::istringstream ss(val_str);
    std::string line;
    std::cerr << prefix(agent_id) << "    BLACKBOARD[" << key << "]:\n";
    while (std::getline(ss, line))
        std::cerr << prefix(agent_id) << "      " << line << "\n";
    writeEntry(agent_id, {{"event","blackboard_write"},{"stage",stage_name},
                          {"key",key},{"value",value}});
}

void AgentLogger::observeDecision(const std::string& agent_id, const std::string& stage_id,
                                   bool done, const std::string& next_action,
                                   const nlohmann::json& observations,
                                   const nlohmann::json& failures)
{
    std::cerr << prefix(agent_id) << "    OBSERVE: done=" << (done ? "true" : "false")
              << " next=" << next_action << "\n";
    if (observations.is_array()) {
        for (const auto& obs : observations)
            if (obs.is_string())
                std::cerr << prefix(agent_id) << "      • " << obs.get<std::string>() << "\n";
    }
    if (failures.is_array() && !failures.empty()) {
        std::cerr << prefix(agent_id) << "    FAILURES:\n";
        for (const auto& f : failures)
            if (f.is_string())
                std::cerr << prefix(agent_id) << "      ✗ " << f.get<std::string>() << "\n";
    }
    writeEntry(agent_id, {{"event","observe_decision"},{"stage_id",stage_id},
                          {"done",done},{"next_action",next_action},
                          {"observations",observations},{"failures",failures}});
}

void AgentLogger::finalAnswer(const std::string& agent_id, const nlohmann::json& answer,
                               int iteration)
{
    std::string ans_str = answer.is_string() ? answer.get<std::string>() : answer.dump(2);
    std::cerr << prefix(agent_id) << "═══ FINAL ANSWER (iteration=" << iteration << ") ═══\n";
    std::istringstream ss(ans_str);
    std::string line;
    while (std::getline(ss, line))
        std::cerr << prefix(agent_id) << "  " << line << "\n";
    writeEntry(agent_id, {{"event","final_answer"},{"answer",answer},{"iteration",iteration}});
}

void AgentLogger::cacheEvent(const std::string& agent_id, const std::string& sub_event,
                              const nlohmann::json& data)
{
    std::cerr << prefix(agent_id) << "    CACHE [" << sub_event << "]";
    if (!data.is_null() && !data.empty()) {
        std::cerr << ":\n";
        std::string d = data.dump(2);
        std::istringstream ss(d);
        std::string line;
        while (std::getline(ss, line))
            std::cerr << prefix(agent_id) << "      " << line << "\n";
    } else {
        std::cerr << "\n";
    }
    writeEntry(agent_id, {{"event","cache_" + sub_event},{"data",data}});
}

} // namespace agent
