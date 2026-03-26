/**
 * llama_demo.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Quick demonstration of llama_functions — mirrors the Python usage examples
 * in ollama_functions.py.
 *
 * Build:
 *   mkdir build && cd build
 *   cmake .. -DLLAMA_DIR=/path/to/llama.cpp
 *   make -j$(nproc)
 *   ./llama_demo /path/to/model.gguf
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "llama_functions.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    // Require a model path on the command line
    if (argc < 2) {
        std::cerr << "Usage: llama_demo <path-to-model.gguf>\n";
        return 1;
    }

    const std::string model_path = argv[1];

    try {
        // ── Load the model ────────────────────────────────────────────────────
        std::cout << "Loading model: " << model_path << "\n";
        LlamaModel model(model_path);
        std::cout << "Model loaded.\n\n";

        // ── generate() — single stateless prompt ─────────────────────────────
        std::cout << "=== generate() ===\n";
        std::string reply = model.generate("What is the capital of France?");
        std::cout << "Reply: " << reply << "\n\n";

        // ── similarity() — cosine similarity between two texts ────────────────
        std::cout << "=== similarity() ===\n";
        float score = model.similarity("cat", "kitten");
        std::cout << "similarity(\"cat\", \"kitten\") = " << score << "\n\n";

        // ── search() — semantic search over a list of candidates ──────────────
        std::cout << "=== search() ===\n";
        auto results = model.search(
            "a fast sports car",
            {"sports car", "bicycle", "cargo ship"},
            {"a high-speed automobile for racing",
             "a human-powered two-wheeled vehicle",
             "a large vessel for transporting goods across oceans"},
            2
        );
        for (const auto& [sc, label] : results) {
            std::cout << "  [" << sc << "] " << label << "\n";
        }
        std::cout << "\n";

        // ── LlamaChat — multi-turn conversation ───────────────────────────────
        std::cout << "=== LlamaChat ===\n";
        LlamaChat conv(model, "You are a concise and helpful assistant.");

        std::string r1 = conv.chat("What is 2 + 2?");
        std::cout << "User: What is 2 + 2?\n";
        std::cout << "Assistant: " << r1 << "\n\n";

        std::string r2 = conv.chat("Why?");
        std::cout << "User: Why?\n";
        std::cout << "Assistant: " << r2 << "\n\n";

        // ── show auto-generated title ─────────────────────────────────────────
        if (auto title = conv.get_title()) {
            std::cout << "Auto-title: " << *title << "\n";
        }

        // ── save and reload history ───────────────────────────────────────────
        std::string saved_path = conv.save_history("demo_chat.json");
        std::cout << "History saved to: " << saved_path << "\n";

        LlamaChat conv2(model);
        conv2.load_history(saved_path);
        std::cout << "History reloaded. Messages: " << conv2.get_history().size() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
