// convo_manager_main.cpp
// Multi-model, multi-agent interactive CLI.
//
// On startup:
//   - loads every agent found in ./agents/ automatically
//   - optionally loads models passed on the command line for direct use
//
// Agent commands:
//   /agent-new                create a new agent interactively
//   /agent-list               list all loaded agents + status
//   /agent-use  <name>        set active agent (typing sends to this agent)
//   /agent-save [name]        save one agent or all
//   /agent-summary [name]     print running summary for one agent or all
//   /agent-info <name>        print full config for an agent
//
// Model/conversation commands (unchanged):
//   /help  /models  /use-model  /new  /list  /use  /save  /close  /quit

#include "ConvoManager.hpp"
#include "agent_manager.hpp"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "Cmdline.hpp"

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Globals for signal handling
// ─────────────────────────────────────────────────────────────────────────────

namespace {

std::atomic_bool             g_should_exit{false};
convo_manager::ConvoManager* g_manager       = nullptr;
AgentManager*                g_agent_manager = nullptr;

void SignalHandler(int) {
    g_should_exit.store(true);
    if (g_agent_manager) g_agent_manager->SaveAllNoThrow();
    if (g_manager)       g_manager->SaveAllNoThrow();
}

// ─────────────────────────────────────────────────────────────────────────────
// PrintHelp
// ─────────────────────────────────────────────────────────────────────────────

void PrintHelp() {
    std::cout <<
        "Agent commands:\n"
        "  /agent-new               create a new agent interactively\n"
        "  /agent-list              list all loaded agents\n"
        "  /agent-use  <name>       set active agent\n"
        "  /agent-save [name]       save one agent or all\n"
        "  /agent-summary [name]    show running summary\n"
        "  /agent-info <name>       show agent config\n"
        "\n"
        "Model commands:\n"
        "  /models                  list loaded models\n"
        "  /use-model <id>          switch active model\n"
        "  /new [title]             new conversation for active model\n"
        "  /list                    list conversations for active model\n"
        "  /use <id>                switch active conversation\n"
        "  /save [path]             save active conversation\n"
        "  /close <id>              close a conversation\n"
        "  /quit                    save all and exit\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// /agent-new — interactive creation
// ─────────────────────────────────────────────────────────────────────────────

void CmdAgentNew(AgentManager& agents) {
    auto ask = [](const std::string& label, const std::string& def = "") {
        if (def.empty()) std::cout << label << ": ";
        else             std::cout << label << " [" << def << "]: ";
        std::string s; std::getline(std::cin, s);
        return s.empty() ? def : s;
    };
    auto ask_int = [&](const std::string& l, int d) {
        try { return std::stoi(ask(l, std::to_string(d))); } catch (...) { return d; }
    };
    auto ask_float = [&](const std::string& l, float d) {
        try { return std::stof(ask(l, std::to_string(d))); } catch (...) { return d; }
    };
    auto ask_bool = [&](const std::string& l, bool d) {
        const std::string s = ask(l, d ? "yes" : "no");
        return (s == "yes" || s == "y" || s == "1" || s == "true");
    };

    std::cout << "\n── New Agent ───────────────────────────────────────\n";
    AgentManager::Config cfg;
    cfg.name = ask("Name");
    if (cfg.name.empty())          { std::cerr << "Name cannot be empty.\n";       return; }
    if (agents.Exists(cfg.name))   { std::cerr << "Agent already exists.\n";       return; }
    cfg.model_path = ask("Model path");
    if (cfg.model_path.empty())    { std::cerr << "Model path cannot be empty.\n"; return; }
    cfg.role                = ask      ("Role (who is this agent?)");
    cfg.instructions        = ask      ("Extra instructions (optional)");
    cfg.context_size        = ask_int  ("Context size",        32768);
    cfg.recent_turns_window = ask_int  ("Recent turns window", 10);
    cfg.clear_kv_each_turn  = ask_bool ("Clear KV each turn",  false);
    cfg.temperature         = ask_float("Temperature",         0.7f);
    cfg.max_tokens          = ask_int  ("Max tokens",          1024);

    try {
        const auto& a = agents.Create(cfg);
        std::cout << "Agent '" << a.config.name << "' created and ready.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// /agent-list
// ─────────────────────────────────────────────────────────────────────────────

void CmdAgentList(const AgentManager& agents, const std::string& active) {
    const auto names = agents.Names();
    if (names.empty()) { std::cout << "No agents loaded.\n"; return; }
    for (const auto& n : names) {
        const auto* a = agents.Get(n);
        std::cout << (n == active ? "* " : "  ")
                  << n
                  << "  model=" << a->config.model_path
                  << "  turns=" << a->turn_count
                  << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// /agent-summary
// ─────────────────────────────────────────────────────────────────────────────

void CmdAgentSummary(const AgentManager& agents, const std::string& name) {
    auto print = [](const AgentManager::Agent& a) {
        std::cout << "[" << a.config.name << "]  turns=" << a.turn_count << "\n"
                  << (a.last_summary.empty() ? "(no summary yet)" : a.last_summary)
                  << "\n";
    };
    if (name.empty()) {
        for (const auto& n : agents.Names()) if (const auto* a = agents.Get(n)) print(*a);
    } else {
        const auto* a = agents.Get(name);
        if (!a) std::cerr << "Unknown agent: " << name << "\n";
        else    print(*a);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// /agent-info
// ─────────────────────────────────────────────────────────────────────────────

void CmdAgentInfo(const AgentManager& agents, const std::string& name) {
    const auto* a = agents.Get(name);
    if (!a) { std::cerr << "Unknown agent: " << name << "\n"; return; }
    const auto& c = a->config;
    std::cout
        << "Name            : " << c.name                             << "\n"
        << "Model           : " << c.model_path                       << "\n"
        << "Role            : " << c.role                             << "\n"
        << "Instructions    : " << c.instructions                     << "\n"
        << "Context size    : " << c.context_size                     << "\n"
        << "Turns window    : " << c.recent_turns_window              << "\n"
        << "Clear KV/turn   : " << (c.clear_kv_each_turn ? "yes":"no")<< "\n"
        << "Temperature     : " << c.temperature                      << "\n"
        << "Max tokens      : " << c.max_tokens                       << "\n"
        << "Folder          : " << a->folder                          << "\n"
        << "Turns completed : " << a->turn_count                      << "\n";
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    convo_manager::ConvoManager manager;
    g_manager = &manager;

    // agents/ directory lives next to the binary.
    const fs::path   bin_dir    = fs::absolute(argv[0]).parent_path();
    const std::string agents_dir = (bin_dir / "agents").string();

    AgentManager agent_mgr(manager, agents_dir);
    g_agent_manager = &agent_mgr;

    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // ── Load all saved agents from disk ──────────────────────────────────────
    const int n_agents = agent_mgr.LoadAll();
    if (n_agents > 0)
        std::cout << "Loaded " << n_agents << " agent(s) from " << agents_dir << "\n";

    // ── Optionally load models from command line ───────────────────────────────
    const auto cmdline = ParseCmdline(argc, argv);
    const std::string system_prompt = cmdline.BaseSystemPrompt;

    std::vector<convo_manager::ModelId> model_ids;
    for (const auto& mp : cmdline.ModelPaths) {
        try {
            const auto mid = manager.AddModel(mp, cmdline.ContextSize);
            model_ids.push_back(mid);
            const auto cid = manager.NewConversation(mid, system_prompt, "default");
            manager.SwitchActiveConversation(mid, cid);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load '" << mp << "': " << e.what() << "\n";
        }
    }

    convo_manager::ModelId active_model =
        model_ids.empty() ? 0 : model_ids.front();

    // active_agent holds the name of the agent that receives typed input.
    std::string active_agent;
    if (active_model == 0 && !agent_mgr.Names().empty())
        active_agent = agent_mgr.Names().front();

    if (!model_ids.empty())
        std::cout << "Loaded " << model_ids.size() << " model(s).\n";
    if (agent_mgr.Names().empty() && model_ids.empty())
        std::cout << "No agents or models loaded. Use /agent-new to create an agent.\n";
    if (!active_agent.empty())
        std::cout << "Active agent: " << active_agent << "\n";

    std::cout << "Type /help for commands.\n";

    // ── REPL ──────────────────────────────────────────────────────────────────
    std::string line;
    while (!g_should_exit.load()) {

        if (!active_agent.empty())
            std::cout << "[agent: " << active_agent << "]> " << std::flush;
        else if (active_model != 0)
            std::cout << "[model " << active_model << "]> " << std::flush;
        else
            std::cout << "[no active]> " << std::flush;

        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        // ── Commands ──────────────────────────────────────────────────────────
        if (line[0] == '/') {
            const auto parts = SplitArgs(line);
            const std::string cmd = parts.empty() ? "" : parts[0];
            try {

                if (cmd == "/agent-new") {
                    CmdAgentNew(agent_mgr);

                } else if (cmd == "/agent-list") {
                    CmdAgentList(agent_mgr, active_agent);

                } else if (cmd == "/agent-use" && parts.size() >= 2) {
                    if (!agent_mgr.Exists(parts[1]))
                        std::cerr << "Unknown agent: " << parts[1] << "\n";
                    else {
                        active_agent = parts[1];
                        std::cout << "Active agent: " << active_agent << "\n";
                    }

                } else if (cmd == "/agent-save") {
                    const std::string name = (parts.size() >= 2) ? parts[1] : "";
                    agent_mgr.Save(name);
                    std::cout << "Saved"
                              << (name.empty() ? " all agents" : " '" + name + "'")
                              << ".\n";

                } else if (cmd == "/agent-summary") {
                    CmdAgentSummary(agent_mgr, parts.size() >= 2 ? parts[1] : "");

                } else if (cmd == "/agent-info" && parts.size() >= 2) {
                    CmdAgentInfo(agent_mgr, parts[1]);

                } else if (cmd == "/help") {
                    PrintHelp();

                } else if (cmd == "/models") {
                    for (const auto& mi : manager.ListModels()) {
                        std::cout << "ModelId=" << mi.Id << " path=" << mi.ModelPath;
                        if (mi.Id == active_model) std::cout << "  [active]";
                        std::cout << "\n";
                    }

                } else if (cmd == "/use-model" && parts.size() >= 2) {
                    active_model = static_cast<convo_manager::ModelId>(std::stoul(parts[1]));
                    active_agent.clear();
                    std::cout << "Active model: " << active_model << "\n";

                } else if (cmd == "/new") {
                    if (active_model == 0) {
                        std::cerr << "No model active.\n";
                    } else {
                        const std::string title =
                            (parts.size() >= 2) ? JoinRemainder(parts, 1) : std::string();
                        const auto cid = manager.NewConversation(
                            active_model, system_prompt,
                            title.empty() ? std::nullopt
                                          : std::optional<std::string>(title));
                        manager.SwitchActiveConversation(active_model, cid);
                        std::cout << "Created convo " << cid << ".\n";
                    }

                } else if (cmd == "/list") {
                    if (active_model == 0) { std::cerr << "No active model.\n"; }
                    else for (const auto& c : manager.ListConversations(active_model)) {
                        std::cout << "ConvoId=" << c.Id
                                  << " dirty="  << (c.Dirty  ? "yes" : "no")
                                  << " closed=" << (c.Closed ? "yes" : "no");
                        if (c.Title) std::cout << " title=\"" << *c.Title << "\"";
                        std::cout << "\n";
                    }

                } else if (cmd == "/use" && parts.size() >= 2) {
                    const auto cid =
                        static_cast<convo_manager::ConvoId>(std::stoul(parts[1]));
                    manager.SwitchActiveConversation(active_model, cid);
                    active_agent.clear();
                    std::cout << "Switched to convo " << cid << "\n";

                } else if (cmd == "/save") {
                    const std::string path = (parts.size() >= 2) ? parts[1] : std::string();
                    const auto active = manager.GetActive();
                    if (!active)
                        std::cerr << "No active conversation.\n";
                    else
                        std::cout << "Saved to "
                                  << manager.Save(active->first, active->second, path)
                                  << "\n";

                } else if (cmd == "/close" && parts.size() >= 2) {
                    const auto cid =
                        static_cast<convo_manager::ConvoId>(std::stoul(parts[1]));
                    manager.Close(active_model, cid);
                    std::cout << "Closed convo " << cid << ".\n";

                } else if (cmd == "/quit") {
                    break;

                } else {
                    std::cerr << "Unknown command. Type /help.\n";
                }

            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
            continue;
        }

        // ── Chat ──────────────────────────────────────────────────────────────
        try {
            std::string reply;
            if (!active_agent.empty()) {
                // Agent path: summary extraction + auto-save happen inside Chat().
                reply = agent_mgr.Chat(active_agent, line);
            } else if (active_model != 0) {
                reply = manager.Chat(active_model, line, 0.7f, cmdline.MaxTokens);
            } else {
                std::cerr << "No active agent or model. Use /agent-new or /agent-use.\n";
                continue;
            }
            std::cout << reply << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────
    agent_mgr.SaveAllNoThrow();
    manager.SaveAllNoThrow();
    return 0;
}
