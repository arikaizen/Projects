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

#include "convoManager/ConvoManager.hpp"
// #include "promt_concat_api.hpp"

#include <atomic>      // std::atomic_bool
#include <csignal>     // std::signal, SIGINT, SIGTERM
#include <filesystem>  // std::filesystem::path
#include <iostream>    // std::cout, std::cerr
#include <optional>    // std::optional
#include <sstream>     // std::istringstream
#include <string>      // std::string
#include <vector>      // std::vector

namespace fs = std::filesystem;

namespace {

std::atomic_bool g_should_exit{false};
convo_manager::ConvoManager* g_manager = nullptr;

void SignalHandler(int) {
  g_should_exit.store(true);
  if (g_manager) {
    g_manager->SaveAllNoThrow();
  }
}

std::vector<std::string> SplitArgs(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string token;
  while (iss >> token) out.push_back(token);
  return out;
}

std::string JoinRemainder(const std::vector<std::string>& parts, std::size_t start) {
  std::string out;
  for (std::size_t i = start; i < parts.size(); ++i) {
    if (i != start) out.push_back(' ');
    out += parts[i];
  }
  return out;
}

struct ParsedCli {
  std::vector<std::string> ModelPaths;
  fs::path PromptListPath;
  std::string BaseSystemPrompt = "You are a helpful assistant.";
  // Default requested: "32k option" (interpreted as 32768 tokens).
  int ContextSize = 32768;
  int MaxTokens = 1024;
};

ParsedCli ParseCli(int argc, char** argv) {
  ParsedCli cli;

  const fs::path exe_path = fs::path(argv[0]);
  const fs::path exe_dir = exe_path.has_parent_path() ? exe_path.parent_path() : fs::current_path();
  cli.PromptListPath = exe_dir / "concat" / "promt_list"; // default if running from AI/
  // If running from AI/bin/, this becomes AI/bin/concat/promt_list (not desired),
  // so we also try ../concat/promt_list later if open fails.

  bool in_models = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--models") { in_models = true; continue; }
    if (a == "--prompt-list" && i + 1 < argc) { cli.PromptListPath = argv[++i]; in_models = false; continue; }
    if (a == "--base" && i + 1 < argc) { cli.BaseSystemPrompt = argv[++i]; in_models = false; continue; }
    if (a == "--ctx" && i + 1 < argc) { cli.ContextSize = std::stoi(argv[++i]); in_models = false; continue; }
    if (a == "--max-tokens" && i + 1 < argc) { cli.MaxTokens = std::stoi(argv[++i]); in_models = false; continue; }

    if (a.rfind("-", 0) == 0) continue; // ignore unknown flags

    if (in_models || argc > 1) {
      cli.ModelPaths.push_back(std::move(a));
    }
  }

  return cli;
}

fs::path ResolvePromptListPath(const fs::path& requested, const fs::path& argv0) {
  // If user passed an explicit path, use it as-is.
  if (requested.is_absolute()) return requested;

  // First try as provided (relative to cwd).
  if (fs::exists(requested)) return requested;

  // Then try relative to executable directory:
  const fs::path exe_path = argv0;
  const fs::path exe_dir = exe_path.has_parent_path() ? exe_path.parent_path() : fs::current_path();
  const fs::path candidate1 = exe_dir / requested;
  if (fs::exists(candidate1)) return candidate1;

  // Common case: binary in AI/bin, list in AI/concat/promt_list
  const fs::path candidate2 = exe_dir / ".." / "concat" / "promt_list";
  if (fs::exists(candidate2)) return candidate2;

  return requested;
}

// Disabled for now: building the system prompt by concatenating files from a list.
// std::string BuildFinalSystemPrompt(const fs::path& prompt_list_path,
//                                   const std::string& base_system_prompt) {
//   prompt_concat::ConcatOptions options;
//   options.IncludeMarkers = true;
//
//   const auto res = prompt_concat::ConcatFromPromptList(prompt_list_path, options);
//   for (const auto& w : res.Warnings) {
//     std::cerr << "[warn] " << w << "\n";
//   }
//   return base_system_prompt + std::string("\n\n") + res.Combined;
// }

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
  const auto cli = ParseCli(argc, argv);
  if (cli.ModelPaths.empty()) {
    std::cerr << "Usage: " << argv[0] << " model1.gguf [model2.gguf ...]\n"
              << "   or: " << argv[0] << " --models model1.gguf model2.gguf --prompt-list AI/concat/promt_list --ctx 32768 --max-tokens 1024\n";
    return 1;
  }

  convo_manager::ConvoManager manager;
  g_manager = &manager;

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  // Disabled for now: prompt concatenation from files.
  // const fs::path prompt_list_path = ResolvePromptListPath(cli.PromptListPath, fs::path(argv[0]));
  // const std::string system_prompt = BuildFinalSystemPrompt(prompt_list_path, cli.BaseSystemPrompt);
  const std::string system_prompt = cli.BaseSystemPrompt;

  // Load all models and create one default conversation per model.
  std::vector<convo_manager::ModelId> model_ids;
  for (const auto& mp : cli.ModelPaths) {
    const auto mid = manager.AddModel(mp, cli.ContextSize);
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
    if (!std::getline(std::cin, line)) break; // Ctrl-D
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

    // Normal chat message
    try {
      const std::string reply = manager.Chat(active_model, line, /*temperature=*/0.7f, cli.MaxTokens);
      std::cout << reply << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n";
    }
  }

  manager.SaveAllNoThrow();
  return 0;
}

