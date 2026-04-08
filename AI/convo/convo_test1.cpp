/**
 * convo_test.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Test suite for convo.hpp / convo.cpp.
 *
 * Structure
 * ─────────
 *   Part 1  — Pure unit tests (no model required).
 *             Tests Role helpers and Message struct.
 *
 *   Part 2  — Integration tests (require a real GGUF model on disk).
 *             Pass the model path as the first command-line argument to run:
 *               ./convo_test /path/to/model.gguf
 *
 * Build (example — adjust paths for your llama.cpp install):
 *   g++ -std=c++17 -O2 \
 *       -I/path/to/llama.cpp/include \
 *       -I/path/to/nlohmann \
 *       convo.cpp convo_test.cpp \
 *       -L/path/to/llama.cpp/build -lllama \
 *       -lpthread -ldl \
 *       -o convo_test
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "convo.hpp"
#include "system_prompt.hpp"

#include <cstdlib>   // (C stdlib helpers)
#include <iostream>  // std::cin, std::cout, std::cerr
#include <string>    // std::string

// ─────────────────────────────────────────────────────────────────────────────
// Lightweight test framework
// ─────────────────────────────────────────────────────────────────────────────


static void test_convo_basic(AIModel& model) {
    std::cout << "test convo"<<std::endl;
    int flag =1;
    std::string reply;
    std::string user_req;
    AIConvo conversation(model, CODER_SYSTEM_PROMPT);
    while(flag)
    {
      std::getline(std::cin, user_req); // Reads until you press Enter
      reply = conversation.Chat(user_req);
      std::cout << reply <<std::endl;
    
    }



}



// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::cout << "convo test suite\n";



    // ── Part 2: integration tests (require a model path) ─────────────────────
    if (argc < 2) {
        std::cout << "\nNo model path provided — skipping integration tests.\n"
                  << "Usage: " << argv[0] << " /path/to/model.gguf\n";
    } else {
        const std::string model_path = argv[1];
        std::cout << "\nLoading model: " << model_path << "\n";

        try {
            AIModel model(model_path);
            std::cout << "Model loaded.\n";

            
            test_convo_basic(model);
           
        } catch (const std::exception& error) {
            std::cerr << "\nFailed to load model: " << error.what() << "\n";
            std::cerr << "Integration tests skipped.\n";
        }
    }
}
