// main.cpp — example driver for LayerInferenceEngine
//
// Demonstrates the three-call workflow:
//   1. Construct with a model path
//   2. calculate() → probe VRAM cost and get max layer count
//   3. load()      → transfer first window to GPU
//   4. generate()  → stream output to stdout with sliding-window inference
//
// Build:  make -C AI/layer_inference
// Run:    ./AI/layer_inference/bin/layer_inference mistral-7b.gguf

#include "layer_inference_engine.h"

#include <cstdio>
#include <iostream>

int main(int argc, char** argv)
{
    const char* model_path = (argc >= 2) ? argv[1] : "mistral-7b.gguf";

    try {
        LayerInferenceEngine engine(model_path);
        engine.set_verbose(true);

        // Probe VRAM and compute the maximum window size for an 8 GB budget.
        int max_layers = engine.calculate(8.0f);
        if (max_layers == 0) {
            std::cerr << "Not enough VRAM for even one layer.\n";
            return 1;
        }

        // Transfer the first window of layers to GPU.
        engine.load(max_layers);

        // Stream the response to stdout (also returned as a string).
        std::string reply = engine.generate("Explain quantum entanglement simply.");

        // Print final performance stats.
        auto s = engine.stats();
        printf("\n\nTokens: %d | %.1f tok/s | peak VRAM: %.0f MB\n",
               s.tokens_generated, s.tokens_per_second, s.peak_vram_mb);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
