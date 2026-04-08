// promt_concat_verify.cpp
// Simple verification tool:
// - reads a promt_list
// - concatenates all files (with markers)
// - prepends a base "system prompt"
// - prints the final prompt so you can visually verify ordering/content

#include "promt_concat_api.hpp"

#include <filesystem>  // std::filesystem::path
#include <iostream>    // std::cout, std::cerr
#include <string>      // std::string

int main(int argc, char** argv) {
  namespace fs = std::filesystem;

  const fs::path exe_path = fs::path(argv[0]);
  const fs::path exe_dir  = exe_path.has_parent_path() ? exe_path.parent_path() : fs::current_path();

  const fs::path prompt_list_path =
      (argc >= 2) ? fs::path(argv[1]) : (exe_dir / ".." / "concat" / "promt_list");

  const std::string base_system_prompt =
      (argc >= 3) ? std::string(argv[2]) : std::string("SYSTEM_PROMPT_BASE");

  try {
    prompt_concat::ConcatOptions options;
    options.IncludeMarkers = true;

    const auto concat_result = prompt_concat::ConcatFromPromptList(prompt_list_path, options);
    for (const auto& w : concat_result.Warnings) {
      std::cerr << "[warn] " << w << "\n";
    }

    const std::string final_prompt =
        base_system_prompt + std::string("\n\n") + concat_result.Combined;

    std::cout << "===== BEGIN SYSTEM PROMPT (BASE) =====\n";
    std::cout << base_system_prompt << "\n";
    std::cout << "===== END SYSTEM PROMPT (BASE) =====\n\n";

    std::cout << "===== BEGIN CONCATENATED FILE CONTENTS =====\n";
    std::cout << concat_result.Combined;
    std::cout << "===== END CONCATENATED FILE CONTENTS =====\n\n";

    std::cout << "===== FINAL PROMPT LENGTH (bytes) =====\n";
    std::cout << final_prompt.size() << "\n";
    std::cout << "===== END =====\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

