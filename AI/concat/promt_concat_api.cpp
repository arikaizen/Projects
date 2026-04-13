#include "promt_concat_api.hpp"

#include <cctype>      // std::isspace
#include <fstream>     // std::ifstream
#include <iterator>    // std::istreambuf_iterator
#include <optional>    // std::optional, std::nullopt
#include <stdexcept>   // std::runtime_error

namespace fs = std::filesystem;

namespace prompt_concat {
namespace {

std::string TrimWhitespace(std::string_view s) {
  std::size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  std::size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return std::string{s.substr(start, end - start)};
}

std::optional<std::string> ReadEntireFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<fs::path> ReadPromptList(const fs::path& prompt_list_path, char comment_prefix) {
  std::ifstream in(prompt_list_path);
  if (!in) {
    throw std::runtime_error("Failed to open list file: " + prompt_list_path.string());
  }

  std::vector<fs::path> files;
  std::string line;
  while (std::getline(in, line)) {
    std::string trimmed = TrimWhitespace(line);
    if (trimmed.empty()) continue;
    if (comment_prefix != '\0' && trimmed[0] == comment_prefix) continue;
    files.emplace_back(std::move(trimmed));
  }

  return files;
}

void AppendWithMarkers(std::string& out, const fs::path& path, std::string_view content) {
  out.append("===== BEGIN FILE: ");
  out.append(path.string());
  out.append(" =====\n");

  out.append(content.data(), content.size());
  if (!out.empty() && out.back() != '\n') out.push_back('\n');

  out.append("===== END FILE: ");
  out.append(path.string());
  out.append(" =====\n\n");
}

} // namespace

ConcatResult ConcatFiles(const std::vector<fs::path>& files, const ConcatOptions& options) {
  ConcatResult result;
  result.Combined.reserve(1024 * 1024);

  for (const auto& path : files) {
    const auto content = ReadEntireFile(path);
    if (!content) {
      result.Warnings.push_back("Could not open/read: " + path.string());
      continue;
    }

    if (options.IncludeMarkers) {
      AppendWithMarkers(result.Combined, path, *content);
    } else {
      result.Combined.append(content->data(), content->size());
      if (!result.Combined.empty() && result.Combined.back() != '\n') result.Combined.push_back('\n');
    }
  }

  if (options.Action) {
    options.Action(result.Combined);
  }

  return result;
}

ConcatResult ConcatFromPromptList(const fs::path& prompt_list_path, const ConcatOptions& options) {
  const auto files = ReadPromptList(prompt_list_path, options.CommentPrefix);
  return ConcatFiles(files, options);
}

} // namespace prompt_concat

