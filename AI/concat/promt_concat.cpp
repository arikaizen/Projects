#include "promt_concat_api.hpp"

#include <filesystem>  // std::filesystem::path
#include <iostream>    // std::cerr

// Backwards-compatible entry point: this file used to be a standalone program.
// It now calls the reusable API in `promt_concat_api.hpp/.cpp`.

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  const fs::path prompt_list_path = (argc >= 2) ? fs::path(argv[1]) : fs::path("promt_list");

  try {
    auto result = prompt_concat::ConcatFromPromptList(prompt_list_path);
    for (const auto& w : result.Warnings) {
      std::cerr << "[warn] " << w << "\n";
    }

    // Leave room for action on the combined string.
    // Example:
    // std::cout << result.Combined;
    (void)result;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}

