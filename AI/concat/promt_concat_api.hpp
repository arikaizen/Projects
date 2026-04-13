#pragma once

#include <filesystem>   // std::filesystem::path
#include <functional>   // std::function
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <vector>       // std::vector

namespace prompt_concat {

struct ConcatOptions {
  // Lines starting with this char (after trimming) are ignored.
  char CommentPrefix = '#';

  // If true, each file is wrapped with BEGIN/END markers in the combined output.
  bool IncludeMarkers = true;

  // If set, called after all files are appended (place for "action on the string").
  std::function<void(std::string&)> Action = nullptr;
};

struct ConcatResult {
  std::string Combined;
  std::vector<std::string> Warnings; // e.g. unreadable files
};

// Reads `prompt_list_path` (one file path per line), reads each file, and concatenates
// their contents into a single string.
//
// - Empty lines are skipped.
// - Trimmed lines starting with `options.comment_prefix` are skipped.
// - Unreadable files do not fail the whole call; they add a warning instead.
//
// Throws std::runtime_error if the prompt list file itself cannot be opened.
ConcatResult ConcatFromPromptList(const std::filesystem::path& prompt_list_path,
                                 const ConcatOptions& options = {});

// Same idea, but takes the file list directly.
ConcatResult ConcatFiles(const std::vector<std::filesystem::path>& files,
                         const ConcatOptions& options = {});

} // namespace prompt_concat

