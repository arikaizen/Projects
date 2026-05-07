// agent_manager.cpp
// Full implementation of AgentManager.
//
// Folder layout (one directory per agent, stored next to the binary):
//
//   agents/
//     templates/
//       default_system_prompt.txt   ← source template with {NAME} {ROLE} {INSTRUCTIONS}
//     <agent-name>/
//       agent.json                  ← config + timestamps
//       system_prompt.txt           ← rendered prompt (template with values substituted)
//       history.json                ← AIConvo message history (JSON)
//       history.json.state.bin      ← llama KV cache binary snapshot
//       summary.json                ← extracted running summary
//       prompt.txt                  ← changing prompt (placeholder)
//
// Summary extraction
// ──────────────────
// The system prompt instructs the model to append:
//   [SUMMARY: one sentence describing the conversation]
// at the end of every reply.  Chat() strips this tag from the displayed text,
// updates Agent::last_summary, increments turn_count, and writes summary.json.

#include "agent_manager.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Module-private helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

std::string NowIso8601() {
    auto now   = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

std::string ReadFile(const fs::path& p) {
    std::ifstream f(p);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + p.string());
    return { std::istreambuf_iterator<char>(f), {} };
}

void WriteFile(const fs::path& p, const std::string& text) {
    std::ofstream f(p);
    if (!f.is_open()) throw std::runtime_error("Cannot write: " + p.string());
    f << text;
}

// Replace every occurrence of key in src with val.
std::string Replace(std::string src, const std::string& key, const std::string& val) {
    size_t pos = 0;
    while ((pos = src.find(key, pos)) != std::string::npos) {
        src.replace(pos, key.size(), val);
        pos += val.size();
    }
    return src;
}

// Trim trailing whitespace and newlines.
void TrimRight(std::string& s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AgentManager::AgentManager(convo_manager::ConvoManager& mgr,
                           const std::string& agents_dir)
    : mgr_(mgr), agents_dir_(fs::absolute(agents_dir))
{
    fs::create_directories(agents_dir_);
    fs::create_directories(agents_dir_ / "templates");
    EnsureTemplateExists();
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadAll — scan agents_dir_ for agent folders and restore each one
// ─────────────────────────────────────────────────────────────────────────────

int AgentManager::LoadAll() {
    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(agents_dir_)) {
        if (!entry.is_directory()) continue;
        if (entry.path().filename() == "templates") continue;

        const fs::path cfg_file = entry.path() / "agent.json";
        if (!fs::exists(cfg_file)) continue;

        try {
            Agent agent = LoadFromFolder(entry.path());
            agents_.emplace(agent.config.name, std::move(agent));
            ++loaded;
        } catch (const std::exception& e) {
            // Non-fatal: report and continue loading other agents.
            fprintf(stderr, "[agent_manager] skipped '%s': %s\n",
                    entry.path().filename().string().c_str(), e.what());
        }
    }
    return loaded;
}

// ─────────────────────────────────────────────────────────────────────────────
// Create — build a new agent folder structure and load it into memory
// ─────────────────────────────────────────────────────────────────────────────

const AgentManager::Agent& AgentManager::Create(const Config& cfg) {
    if (cfg.name.empty())
        throw std::invalid_argument("AgentManager::Create: name must not be empty");
    if (cfg.model_path.empty())
        throw std::invalid_argument("AgentManager::Create: model_path must not be empty");
    if (agents_.count(cfg.name))
        throw std::runtime_error("AgentManager::Create: agent '" + cfg.name + "' already exists");

    const fs::path folder = agents_dir_ / cfg.name;
    fs::create_directories(folder);

    // Load (or reuse) the model.
    Agent agent;
    agent.config    = cfg;
    agent.folder    = folder.string();
    agent.model_id  = GetOrLoadModel(cfg.model_path, cfg.context_size);

    // Create the conversation with the rendered system prompt.
    const std::string sys = RenderSystemPrompt(cfg);
    agent.convo_id = mgr_.NewConversation(agent.model_id, sys, cfg.name);
    mgr_.SetAgentConfig(agent.model_id, agent.convo_id,
                        cfg.recent_turns_window, cfg.clear_kv_each_turn);

    // Write all files to disk (no history or state yet — brand new agent).
    WriteFile(folder / "system_prompt.txt", sys);
    WriteFile(folder / "prompt.txt", "");       // placeholder for changing prompt
    WriteAllFiles(agent);                        // agent.json + summary.json

    agents_.emplace(cfg.name, std::move(agent));
    return agents_.at(cfg.name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat — send a message to a named agent, extract summary, auto-save
// ─────────────────────────────────────────────────────────────────────────────

std::string AgentManager::Chat(const std::string& name, const std::string& message) {
    auto it = agents_.find(name);
    if (it == agents_.end())
        throw std::runtime_error("AgentManager::Chat: unknown agent '" + name + "'");
    Agent& agent = it->second;

    std::string reply = mgr_.Chat(agent.model_id, agent.convo_id, message,
                                   agent.config.temperature, agent.config.max_tokens);

    // Strip [SUMMARY: ...] from reply, update agent's summary state.
    const std::string clean = ExtractSummary(reply, agent);

    // Auto-save after every turn so state survives crashes.
    try {
        const std::string hist = (fs::path(agent.folder) / "history.json").string();
        mgr_.Save(agent.model_id, agent.convo_id, hist);
        WriteSummary(agent);
        WriteConfig(agent);
    } catch (const std::exception& e) {
        fprintf(stderr, "[agent_manager] auto-save failed for '%s': %s\n",
                name.c_str(), e.what());
    }

    return clean;
}

// ─────────────────────────────────────────────────────────────────────────────
// Save / SaveAllNoThrow
// ─────────────────────────────────────────────────────────────────────────────

void AgentManager::Save(const std::string& name) {
    if (name.empty()) {
        for (auto& [n, agent] : agents_) {
            const std::string hist = (fs::path(agent.folder) / "history.json").string();
            mgr_.Save(agent.model_id, agent.convo_id, hist);
            WriteSummary(agent);
            WriteConfig(agent);
        }
    } else {
        auto it = agents_.find(name);
        if (it == agents_.end())
            throw std::runtime_error("AgentManager::Save: unknown agent '" + name + "'");
        Agent& agent = it->second;
        const std::string hist = (fs::path(agent.folder) / "history.json").string();
        mgr_.Save(agent.model_id, agent.convo_id, hist);
        WriteSummary(agent);
        WriteConfig(agent);
    }
}

void AgentManager::SaveAllNoThrow() noexcept {
    try { Save(); } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// Queries
// ─────────────────────────────────────────────────────────────────────────────

bool AgentManager::Exists(const std::string& name) const {
    return agents_.count(name) > 0;
}

const AgentManager::Agent* AgentManager::Get(const std::string& name) const {
    auto it = agents_.find(name);
    return (it != agents_.end()) ? &it->second : nullptr;
}

std::vector<std::string> AgentManager::Names() const {
    std::vector<std::string> out;
    out.reserve(agents_.size());
    for (const auto& [n, _] : agents_) out.push_back(n);
    std::sort(out.begin(), out.end());
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: GetOrLoadModel
// ─────────────────────────────────────────────────────────────────────────────

convo_manager::ModelId AgentManager::GetOrLoadModel(const std::string& path, int ctx_size) {
    // Normalise to absolute path so two relative paths to the same file match.
    const std::string key = fs::absolute(path).string();
    auto it = model_cache_.find(key);
    if (it != model_cache_.end()) return it->second;

    const convo_manager::ModelId mid = mgr_.AddModel(path, ctx_size);
    model_cache_.emplace(key, mid);
    return mid;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: RenderSystemPrompt
// ─────────────────────────────────────────────────────────────────────────────

std::string AgentManager::RenderSystemPrompt(const Config& cfg) const {
    const fs::path tpl_path = agents_dir_ / "templates" / "default_system_prompt.txt";
    std::string tpl;
    try {
        tpl = ReadFile(tpl_path);
    } catch (...) {
        // Fallback if template missing.
        tpl = "You are {NAME}.\n{ROLE}\n{INSTRUCTIONS}\n"
              "After each response append:\n"
              "[SUMMARY: one sentence about the conversation so far]\n";
    }
    tpl = Replace(tpl, "{NAME}",         cfg.name);
    tpl = Replace(tpl, "{ROLE}",         cfg.role);
    tpl = Replace(tpl, "{INSTRUCTIONS}", cfg.instructions);
    return tpl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: WriteAllFiles — write agent.json + summary.json (no history/state)
// ─────────────────────────────────────────────────────────────────────────────

void AgentManager::WriteAllFiles(const Agent& agent) {
    WriteConfig(agent);
    WriteSummary(agent);
}

void AgentManager::WriteConfig(const Agent& agent) {
    const fs::path p = fs::path(agent.folder) / "agent.json";
    json j;
    j["name"]                = agent.config.name;
    j["model_path"]          = agent.config.model_path;
    j["role"]                = agent.config.role;
    j["instructions"]        = agent.config.instructions;
    j["context_size"]        = agent.config.context_size;
    j["recent_turns_window"] = agent.config.recent_turns_window;
    j["clear_kv_each_turn"]  = agent.config.clear_kv_each_turn;
    j["temperature"]         = agent.config.temperature;
    j["max_tokens"]          = agent.config.max_tokens;
    j["last_active"]         = NowIso8601();
    WriteFile(p, j.dump(2));
}

void AgentManager::WriteSummary(const Agent& agent) {
    const fs::path p = fs::path(agent.folder) / "summary.json";
    json j;
    j["summary"]      = agent.last_summary;
    j["turn_count"]   = agent.turn_count;
    j["last_updated"] = NowIso8601();
    WriteFile(p, j.dump(2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: LoadFromFolder — reconstruct an Agent from its folder on disk
// ─────────────────────────────────────────────────────────────────────────────

AgentManager::Agent AgentManager::LoadFromFolder(const fs::path& folder) {
    // Read agent.json.
    const json j = json::parse(ReadFile(folder / "agent.json"));

    Config cfg;
    cfg.name                = j.at("name").get<std::string>();
    cfg.model_path          = j.at("model_path").get<std::string>();
    cfg.role                = j.value("role", "");
    cfg.instructions        = j.value("instructions", "");
    cfg.context_size        = j.value("context_size",        32768);
    cfg.recent_turns_window = j.value("recent_turns_window", 10);
    cfg.clear_kv_each_turn  = j.value("clear_kv_each_turn",  false);
    cfg.temperature         = j.value("temperature",         0.7f);
    cfg.max_tokens          = j.value("max_tokens",          1024);

    // Load (or reuse) the model.
    Agent agent;
    agent.config   = cfg;
    agent.folder   = folder.string();
    agent.model_id = GetOrLoadModel(cfg.model_path, cfg.context_size);

    // Read the rendered system prompt.
    const fs::path sp_path = folder / "system_prompt.txt";
    std::string sys = fs::exists(sp_path)
        ? ReadFile(sp_path)
        : RenderSystemPrompt(cfg);

    // Create the conversation slot.
    agent.convo_id = mgr_.NewConversation(agent.model_id, sys, cfg.name);
    mgr_.SetAgentConfig(agent.model_id, agent.convo_id,
                        cfg.recent_turns_window, cfg.clear_kv_each_turn);

    // Restore message history.
    const fs::path hist = folder / "history.json";
    if (fs::exists(hist))
        mgr_.LoadConvoHistory(agent.model_id, agent.convo_id, hist.string());

    // Restore KV cache state.
    const fs::path state = folder / "history.json.state.bin";
    if (fs::exists(state))
        mgr_.LoadConvoState(agent.model_id, agent.convo_id, state.string());

    // Load summary metadata.
    const fs::path sum_path = folder / "summary.json";
    if (fs::exists(sum_path)) {
        try {
            const json sj    = json::parse(ReadFile(sum_path));
            agent.last_summary = sj.value("summary",    "");
            agent.turn_count   = sj.value("turn_count", 0);
        } catch (...) {}
    }

    return agent;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: ExtractSummary — strip [SUMMARY: ...] from reply, update agent state
// ─────────────────────────────────────────────────────────────────────────────

std::string AgentManager::ExtractSummary(std::string& reply, Agent& agent) {
    const std::string tag = "[SUMMARY:";
    const size_t pos = reply.rfind(tag);
    if (pos == std::string::npos) return reply;

    const size_t close = reply.find(']', pos);
    if (close == std::string::npos) return reply;

    // Extract the summary text.
    std::string summary = reply.substr(pos + tag.size(), close - pos - tag.size());
    TrimRight(summary);
    while (!summary.empty() && (summary.front() == ' ' || summary.front() == '\t'))
        summary.erase(0, 1);

    agent.last_summary = summary;
    ++agent.turn_count;

    // Remove the summary line from the reply shown to the user.
    std::string clean = reply.substr(0, pos);
    TrimRight(clean);
    return clean;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: EnsureTemplateExists — write default template if missing
// ─────────────────────────────────────────────────────────────────────────────

void AgentManager::EnsureTemplateExists() const {
    const fs::path tpl = agents_dir_ / "templates" / "default_system_prompt.txt";
    if (fs::exists(tpl)) return;

    const std::string content =
        "You are {NAME}, an AI agent.\n\n"
        "{ROLE}\n\n"
        "{INSTRUCTIONS}\n\n"
        "After every response append exactly this line at the very end:\n"
        "[SUMMARY: one sentence describing what this conversation has covered so far]\n"
        "Omit the summary line if this is your very first response.\n";

    WriteFile(tpl, content);
}
