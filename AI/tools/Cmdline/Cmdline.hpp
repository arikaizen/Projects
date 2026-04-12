#pragma once

#include <filesystem>  // std::filesystem::path
#include <string>      // std::string
#include <vector>      // std::vector

std::vector<std::string> SplitArgs(const std::string& line);
std::string JoinRemainder(const std::vector<std::string>& parts, std::size_t start);

struct ParsedCmdline {
  std::vector<std::string> ModelPaths;
  std::filesystem::path PromptListPath;
  std::string BaseSystemPrompt = "You are a helpful assistant.";
  int ContextSize = 32768;
  int MaxTokens = 1024;
};

ParsedCmdline ParseCmdline(int argc, char** argv);

