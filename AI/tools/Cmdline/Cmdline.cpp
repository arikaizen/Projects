#include "Cmdline.hpp"

#include <sstream>   // std::istringstream
#include <string>    // std::string
#include <utility>   // std::move
#include <vector>    // std::vector

namespace fs = std::filesystem;

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

ParsedCmdline ParseCmdline(int argc, char** argv) {
  ParsedCmdline cmdline;

  const fs::path exe_path = fs::path(argv[0]);
  const fs::path exe_dir = exe_path.has_parent_path() ? exe_path.parent_path() : fs::current_path();
  cmdline.PromptListPath = exe_dir / "concat" / "promt_list";

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--models") { continue; }
    if (a == "--prompt-list" && i + 1 < argc) { cmdline.PromptListPath = argv[++i]; continue; }
    if (a == "--base" && i + 1 < argc) { cmdline.BaseSystemPrompt = argv[++i]; continue; }
    if (a == "--ctx" && i + 1 < argc) { cmdline.ContextSize = std::stoi(argv[++i]); continue; }
    if (a == "--max-tokens" && i + 1 < argc) { cmdline.MaxTokens = std::stoi(argv[++i]); continue; }

    if (a.rfind("-", 0) == 0) continue;
    cmdline.ModelPaths.push_back(std::move(a));
  }

  return cmdline;
}

