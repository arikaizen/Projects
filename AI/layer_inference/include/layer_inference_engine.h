#pragma once
// layer_inference_engine.h
// Public interface for the sliding-window GPU inference engine.
// Zero llama.cpp / ggml includes here — all implementation details are in the .cpp
// via the pimpl idiom.

#include <memory>
#include <string>

class LayerInferenceEngine {
public:

    // Purpose : Open the GGUF file and read model metadata only.
    //           No layers are loaded, no GPU memory is allocated yet.
    // Args    : model_path — path to a .gguf model file
    // Throws  : std::runtime_error if the file cannot be opened or is not valid GGUF
    explicit LayerInferenceEngine(const std::string& model_path);

    // Purpose : Measure the real VRAM cost of one transformer block layer, then
    //           compute the maximum number of layers that fit inside the budget.
    //           Stores the result so load() can use it without re-measuring.
    // Args    : vram_budget_gb — gigabytes of VRAM to use (e.g. 8.0 for 8 GB)
    // Returns : max_layers — largest window size that fits; 0 if nothing fits
    int calculate(float vram_budget_gb);

    // Purpose : Allocate the GPU window and fill it with the first window_size
    //           transformer block layers.  After this call the engine is ready
    //           to generate.  Must be called after calculate().
    // Args    : window_size — number of layers to keep on GPU simultaneously;
    //                         must be <= value returned by calculate()
    // Throws  : std::runtime_error if calculate() has not been called, or if
    //           window_size > max_layers returned by calculate()
    void load(int window_size);

    // Purpose : Tokenise the prompt, run it through all model layers using the
    //           sliding window, and stream generated tokens to stdout one by one.
    //           The window slides one layer at a time: after each layer's forward
    //           pass, that layer is evicted and the next unseen layer is loaded,
    //           keeping exactly window_size layers resident at all times.
    // Args    : prompt     — user's input string
    //           max_tokens — stop after this many new tokens (0 = use model default)
    // Returns : the full generated string (same text streamed to stdout)
    // Throws  : std::runtime_error if load() has not been called yet
    std::string generate(const std::string& prompt, int max_tokens = 0);

    // Purpose : Toggle per-layer stderr logging on or off after construction.
    // Args    : on — true to enable verbose output, false to silence it
    void set_verbose(bool on);

    // Stats snapshot from the most recent generate() call.
    struct Stats {
        int   tokens_generated;
        float total_ms;
        float tokens_per_second;
        float peak_vram_mb;
        float avg_layer_ms;
    };

    // Purpose : Return a snapshot of counters from the last generate() call.
    // Returns : Stats struct; zero-initialised before the first generate() call
    Stats stats() const;

    ~LayerInferenceEngine();

    // Non-copyable, non-movable (owns GPU resources)
    LayerInferenceEngine(const LayerInferenceEngine&)            = delete;
    LayerInferenceEngine& operator=(const LayerInferenceEngine&) = delete;
    LayerInferenceEngine(LayerInferenceEngine&&)                 = delete;
    LayerInferenceEngine& operator=(LayerInferenceEngine&&)      = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
