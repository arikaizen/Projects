// convo_manager_main.cpp
// Multi-model, multi-conversation interactive CLI.
//
// Usage examples:
//   ./AI/bin/convo_manager model1.gguf model2.gguf
//   ./AI/bin/convo_manager --models model1.gguf model2.gguf --prompt-list AI/concat/promt_list
//
// Commands (type in the prompt):
//   /help
//   /models
//   /use-model <modelId>
//   /new [title...]
//   /list
//   /use <convoId>
//   /save [path]
//   /close <convoId>
//   /quit

#include "ConvoManager.hpp"

#include <atomic>      // std::atomic_bool
#include <csignal>     // std::signal, SIGINT, SIGTERM
#include <iostream>    // std::cout, std::cerr
#include <optional>    // std::optional
#include <string>      // std::string
#include <vector>      // std::vector

#include "Cmdline.hpp"

namespace {

std::atomic_bool g_should_exit{false};
convo_manager::ConvoManager* g_manager = nullptr;

void SignalHandler(int) {
  g_should_exit.store(true);
  if (g_manager) {
    g_manager->SaveAllNoThrow();
  }
}

void PrintHelp() {
  std::cout
    << "Commands:\n"
    << "  /help\n"
    << "  /models\n"
    << "  /use-model <modelId>\n"
    << "  /new [title...]\n"
    << "  /list\n"
    << "  /use <convoId>\n"
    << "  /save [path]\n"
    << "  /close <convoId>\n"
    << "  /quit\n";
}

} // namespace

int main(int argc, char** argv) {
  convo_manager::ConvoManager manager;
  g_manager = &manager;

  const auto cmdline = ParseCmdline(argc, argv);
  if (cmdline.ModelPaths.empty()) {
    std::cerr << "Usage: " << argv[0] << " model1.gguf [model2.gguf ...]\n"
              << "   or: " << argv[0] << " --models model1.gguf model2.gguf --ctx 32768 --max-tokens 1024\n";
    return 1;
  }

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  const std::string system_prompt = cmdline.BaseSystemPrompt;

  std::vector<convo_manager::ModelId> model_ids;
  for (const auto& mp : cmdline.ModelPaths) {
    const auto mid = manager.AddModel(mp, cmdline.ContextSize);
    model_ids.push_back(mid);
    const auto cid = manager.NewConversation(mid, system_prompt, std::string("default"));
    manager.SwitchActiveConversation(mid, cid);
  }

  convo_manager::ModelId active_model = model_ids.front();

  std::cout << "Loaded " << model_ids.size() << " model(s).\n";
  std::cout << "Active model: " << active_model << "\n";
  std::cout << "Type /help for commands.\n";

  std::string line;
  while (!g_should_exit.load()) {
    std::cout << "[model " << active_model << "]> " << std::flush;
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    if (line[0] == '/') {
      const auto parts = SplitArgs(line);
      const std::string cmd = parts.empty() ? "" : parts[0];

      try {
        if (cmd == "/help") {
          PrintHelp();
        } else if (cmd == "/models") {
          for (const auto& mi : manager.ListModels()) {
            std::cout << "ModelId=" << mi.Id << " path=" << mi.ModelPath;
            if (mi.Id == active_model) std::cout << "  [active]";
            std::cout << "\n";
          }
        } else if (cmd == "/use-model" && parts.size() >= 2) {
          active_model = static_cast<convo_manager::ModelId>(std::stoul(parts[1]));
          std::cout << "Active model set to " << active_model << "\n";
        } else if (cmd == "/new") {
          const std::string title = (parts.size() >= 2) ? JoinRemainder(parts, 1) : std::string();
          const auto cid = manager.NewConversation(active_model, system_prompt,
                                                   title.empty() ? std::nullopt : std::optional<std::string>(title));
          manager.SwitchActiveConversation(active_model, cid);
          std::cout << "Created convo " << cid << " and switched to it.\n";
        } else if (cmd == "/list") {
          const auto convos = manager.ListConversations(active_model);
          for (const auto& c : convos) {
            std::cout << "ConvoId=" << c.Id
                      << " dirty=" << (c.Dirty ? "yes" : "no")
                      << " closed=" << (c.Closed ? "yes" : "no");
            if (c.Title.has_value()) std::cout << " title=\"" << *c.Title << "\"";
            if (c.LastSavedPath.has_value()) std::cout << " saved=\"" << *c.LastSavedPath << "\"";
            std::cout << "\n";
          }
        } else if (cmd == "/use" && parts.size() >= 2) {
          const auto cid = static_cast<convo_manager::ConvoId>(std::stoul(parts[1]));
          manager.SwitchActiveConversation(active_model, cid);
          std::cout << "Switched to convo " << cid << "\n";
        } else if (cmd == "/save") {
          const std::string path = (parts.size() >= 2) ? parts[1] : std::string();
          const auto active = manager.GetActive();
          if (!active.has_value() || active->first != active_model) {
            std::cerr << "No active conversation for this model.\n";
          } else {
            const std::string written = manager.Save(active_model, active->second, path);
            std::cout << "Saved to " << written << "\n";
          }
        } else if (cmd == "/close" && parts.size() >= 2) {
          const auto cid = static_cast<convo_manager::ConvoId>(std::stoul(parts[1]));
          manager.Close(active_model, cid);
          std::cout << "Closed convo " << cid << " (saved best-effort).\n";
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

    try {
      const std::string reply = manager.Chat(active_model, line, /*temperature=*/0.7f, cmdline.MaxTokens);
      std::cout << reply << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n";
    }
  }

  manager.SaveAllNoThrow();
  return 0;
}

