// promt_concat_test.cpp
// Builds a system prompt from promt_list, then starts an interactive convo loop.

#include "convo.hpp"
#include "promt_concat_api.hpp"

#include <filesystem> // std::filesystem::path
#include <iostream>  // std::cin, std::cout, std::cerr
#include <string>    // std::string

static int RunInteractiveChat(const std::string& model_path,
                              const std::filesystem::path& prompt_list_path) {
  AIModel model(model_path);

  const auto concat_result = prompt_concat::ConcatFromPromptList(prompt_list_path);
  for (const auto& w : concat_result.Warnings) {
    std::cerr << "[warn] " << w << "\n";
  }

  const std::string system_prompt =
      std::string("You are a helpful assistant.") + std::string("\n\n") + concat_result.Combined;

  AIConvo conversation(model, system_prompt);

  std::cout << "Interactive chat. Ctrl-D to exit.\n";
  std::string user_req;
  while (std::getline(std::cin, user_req)) {
    const std::string reply = conversation.Chat(user_req);
    std::cout << reply << "\n";
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " /path/to/model.gguf [path/to/promt_list]\n";
    return 1;
  }

  try {
    namespace fs = std::filesystem;

    const fs::path exe_path = fs::path(argv[0]);
    const fs::path exe_dir  = exe_path.has_parent_path() ? exe_path.parent_path() : fs::current_path();

    const fs::path prompt_list_path =
        (argc >= 3) ? fs::path(argv[2]) : (exe_dir / ".." / "concat" / "promt_list");

    return RunInteractiveChat(argv[1], prompt_list_path);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
