#pragma once
// agent_manager.hpp
// Manages the full lifecycle of named AI agents stored on disk.
//
// Each agent lives in agents/<name>/ and owns:
//   agent.json          — config: model path, parameters, timestamps
//   system_prompt.txt   — rendered system prompt (role + instructions)
//   history.json        — full conversation history (every turn, text)
//   history.json.state.bin — KV cache binary (exact model memory state)
//   summary.json        — auto-updated running summary extracted from replies
//   prompt.txt          — changing prompt placeholder (future use)
//
// Usage:
//   ConvoManager mgr;
//   AgentManager agents(mgr, "agents");
//   agents.LoadAll();                              // restore all saved agents
//   agents.Create({ "researcher", "model.gguf",   // create a new agent
//                   "You research science.", "" });
//   std::string reply = agents.Chat("researcher", "What is entropy?");
//   agents.Save();                                 // save all to disk

#include "ConvoManager.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class AgentManager {
public:

    // ── Per-agent configuration ───────────────────────────────────────────────
    // Written to agent.json and used to reconstruct the agent on next load.
    struct Config {
        std::string name;
        std::string model_path;
        std::string role;                       // inserted into system prompt
        std::string instructions;               // inserted into system prompt
        int   context_size        = 32768;
        int   recent_turns_window = 10;
        bool  clear_kv_each_turn  = false;
        float temperature         = 0.7f;
        int   max_tokens          = 1024;
    };

    // ── Runtime state per loaded agent ────────────────────────────────────────
    struct Agent {
        Config                        config;
        convo_manager::ModelId        model_id  = 0;
        convo_manager::ConvoId        convo_id  = 0;
        std::string                   folder;       // absolute path to agent dir
        std::string                   last_summary;
        int                           turn_count = 0;
    };

    // ── Construction ──────────────────────────────────────────────────────────
    // agents_dir is relative to the working directory (next to the binary).
    explicit AgentManager(convo_manager::ConvoManager& mgr,
                          const std::string& agents_dir = "agents");

    // ── Startup: load all agents found on disk ────────────────────────────────
    // Returns the number of agents successfully loaded.
    int LoadAll();

    // ── Create a new agent (writes files + loads immediately) ─────────────────
    // Throws if an agent with that name already exists.
    const Agent& Create(const Config& cfg);

    // ── Talk to a named agent ─────────────────────────────────────────────────
    // Extracts [SUMMARY: ...] from the reply, updates summary.json,
    // auto-saves history + KV state, and returns the clean reply text.
    std::string Chat(const std::string& name, const std::string& message);

    // ── Run multiple agents concurrently ──────────────────────────────────────
    struct AgentRequest { std::string name; std::string message; };
    struct AgentResult  {
        std::string name;
        std::string reply;
        std::string error;  // non-empty if the turn failed
    };
    // Launches one thread per request.  Each agent name must be unique in the batch.
    // Never throws; per-agent errors go into AgentResult::error.
    std::vector<AgentResult> ChatBatch(const std::vector<AgentRequest>& requests);

    // ── Persistence ───────────────────────────────────────────────────────────
    // Save one named agent, or every agent if name is empty.
    void Save(const std::string& name = "");
    void SaveAllNoThrow() noexcept;

    // ── Queries ───────────────────────────────────────────────────────────────
    bool               Exists(const std::string& name) const;
    const Agent*       Get(const std::string& name)    const;
    std::vector<std::string> Names()                   const;
    const std::unordered_map<std::string, Agent>& All() const { return agents_; }

private:
    convo_manager::ConvoManager&                     mgr_;
    std::filesystem::path                            agents_dir_;
    std::unordered_map<std::string, Agent>           agents_;
    std::unordered_map<std::string, convo_manager::ModelId> model_cache_;

    // ── Internal helpers ──────────────────────────────────────────────────────
    convo_manager::ModelId GetOrLoadModel(const std::string& path, int ctx_size);
    std::string            RenderSystemPrompt(const Config& cfg) const;
    void                   WriteAllFiles(const Agent& agent);
    void                   WriteConfig(const Agent& agent);
    void                   WriteSummary(const Agent& agent);
    Agent                  LoadFromFolder(const std::filesystem::path& folder);
    std::string            ExtractSummary(std::string& reply, Agent& agent);
    void                   EnsureTemplateExists() const;
};
