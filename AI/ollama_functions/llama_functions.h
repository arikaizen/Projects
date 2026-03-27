/**
 * llama_functions.h
 * ─────────────────────────────────────────────────────────────────────────────
 * C++ library that mirrors ollama_functions.py using llama.cpp directly.
 *
 * Provides:
 *   - Role enum and Message struct (same semantics as Python's Literal / TypedDict)
 *   - LlamaModel class: stateless inference — generate(), embed(), similarity(), search()
 *   - LlamaChat class:  stateful conversation — chat(), history management, persistence
 *
 * Dependencies:
 *   - llama.cpp  (llama.h)
 *   - nlohmann/json  (nlohmann/json.hpp)  — for save_history / load_history
 *   - Standard C++17
 *
 * Build with CMakeLists.txt included in this repository.
 *
 * Quick start:
 *   LlamaModel model("path/to/model.gguf");
 *   std::string reply = model.generate("Hello, world!");
 *
 *   LlamaChat chat(model);
 *   std::string r1 = chat.chat("What is 2 + 2?");
 *   std::string r2 = chat.chat("Why?");
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

// ── Standard library ──────────────────────────────────────────────────────────
#include <cstdint>       // uint32_t
#include <optional>      // std::optional
#include <stdexcept>     // std::runtime_error, std::invalid_argument
#include <string>        // std::string
#include <unordered_map> // std::unordered_map  (embedding cache)
#include <utility>       // std::pair
#include <vector>        // std::vector

// ── llama.cpp public C header ─────────────────────────────────────────────────
#include <llama.h>

// ── JSON serialisation (header-only, single file) ─────────────────────────────
#include <nlohmann/json.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Role — the three possible speakers in a conversation (mirrors Python Literal)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Represents which participant authored a message.
 *
 *   System    — the invisible instruction that shapes the model's behaviour
 *   User      — the human turn
 *   Assistant — the model's reply
 */
enum class Role : uint8_t {
    System,
    User,
    Assistant,
};

/** Convert a Role to the lowercase string expected by llama_chat_apply_template. */
std::string role_to_string(Role role);

/** Parse a lowercase string back to a Role (throws std::invalid_argument if unknown). */
Role role_from_string(const std::string& s);

// ─────────────────────────────────────────────────────────────────────────────
// Message — one turn in a conversation (mirrors Python TypedDict)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * A single message exchanged between a participant and the model.
 *
 * Fields:
 *   role    — who wrote this message
 *   content — the raw text of the message
 */
struct Message {
    Role        role;
    std::string content;
};

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — stateless inference over a GGUF model file
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Wraps a llama.cpp model and context, exposing stateless inference helpers.
 *
 * One LlamaModel object corresponds to one GGUF model loaded into memory.
 * Multiple LlamaChat objects can reference the same LlamaModel to share weights.
 *
 * Not copyable (the llama_model/llama_context pointers are owned resources).
 *
 * Construction:
 *   LlamaModel model("mistral-7b.gguf");
 *   LlamaModel model("mistral-7b.gguf", 2048, 4);  // custom ctx_size, threads
 */
class LlamaModel {
public:
    // ── Construction / destruction ────────────────────────────────────────────

    /**
     * Load a GGUF model file from disk.
     *
     * @param model_path  Absolute or relative path to the .gguf file.
     * @param ctx_size    Context window size in tokens (default: 4096).
     * @param n_threads   CPU threads to use for inference (default: 4).
     *
     * @throws std::runtime_error if the model or context cannot be loaded.
     */
    explicit LlamaModel(const std::string& model_path,
                        int                ctx_size  = 4096,
                        int                n_threads = 4);

    /**
     * Free the llama context and model, and unload the llama backend.
     * Destructor is noexcept — any cleanup errors are silently ignored.
     */
    ~LlamaModel() noexcept;

    // Non-copyable — model pointers are unique owned resources
    LlamaModel(const LlamaModel&)            = delete;
    LlamaModel& operator=(const LlamaModel&) = delete;

    // Movable — safe to transfer ownership
    LlamaModel(LlamaModel&&) noexcept;
    LlamaModel& operator=(LlamaModel&&) noexcept;

    // ── Stateless inference ───────────────────────────────────────────────────

    /**
     * Generate a single response to a plain-text prompt (no conversation history).
     * Mirrors Python's generate().
     *
     * @param prompt      The text prompt to send to the model.
     * @param temperature Sampling temperature in [0.0, 2.0]. Default 0.7.
     *                    Lower = more deterministic, higher = more creative.
     * @param max_tokens  Maximum tokens to generate. Must be >= 1. Default 2048.
     *
     * @returns The model's response as a UTF-8 string.
     *
     * @throws std::invalid_argument if prompt is blank, temperature out of range,
     *                               or max_tokens < 1.
     * @throws std::runtime_error    if the model produces an empty response or
     *                               inference fails internally.
     *
     * Example:
     *   std::string reply = model.generate("What is the capital of France?");
     */
    std::string generate(const std::string& prompt,
                         float              temperature = 0.7f,
                         int                max_tokens  = 2048) const;

    /**
     * Convert a string of text into a dense embedding vector.
     * Mirrors Python's embed().
     *
     * Results are cached by text so repeated calls with the same input are free.
     *
     * @param text      The text to embed. Must not be blank.
     * @param use_cache If true (default), return a cached vector when available
     *                  and store the result after computing it.
     *
     * @returns A float vector of length equal to the model's embedding dimension.
     *
     * @throws std::invalid_argument if text is blank.
     * @throws std::runtime_error    if the embedding is all-zeros (zero-magnitude).
     *
     * Example:
     *   std::vector<float> vec = model.embed("Hello, world!");
     */
    std::vector<float> embed(const std::string& text,
                             bool               use_cache = true);

    /**
     * Compute the cosine similarity between two pieces of text.
     * Mirrors Python's similarity().
     *
     * Returns a score in [-1.0, 1.0]:
     *   1.0  — identical meaning
     *   0.0  — unrelated
     *  -1.0  — opposite meaning
     *
     * @param text_a First text.
     * @param text_b Second text.
     *
     * @returns Cosine similarity score as a float.
     *
     * @throws std::invalid_argument if either text is blank.
     * @throws std::runtime_error    if either embedding has zero magnitude.
     *
     * Example:
     *   float score = model.similarity("cat", "kitten");
     */
    float similarity(const std::string& text_a, const std::string& text_b);

    /**
     * Find the most semantically similar labels for a query.
     * Mirrors Python's search().
     *
     * Embeds the query and each text, ranks by cosine similarity, and returns
     * the top_n (label, score) pairs in descending order.
     *
     * @param query   The search query.
     * @param labels  Human-readable names for each candidate (e.g. category names).
     * @param texts   Full text for each candidate (same length as labels).
     * @param top_n   How many results to return. Default 3.
     *
     * @returns Vector of (score, label) pairs, highest score first.
     *
     * @throws std::invalid_argument if query is blank, labels/texts differ in
     *                               length, or top_n < 1.
     *
     * Example:
     *   auto results = model.search("fast car", {"sports car", "bicycle", "boat"},
     *                                           {"a high-speed automobile", ...},
     *                               2);
     */
    std::vector<std::pair<float, std::string>>
    search(const std::string&              query,
           const std::vector<std::string>& labels,
           const std::vector<std::string>& texts,
           int                             top_n = 3);

    // ── Accessors ─────────────────────────────────────────────────────────────

    /** Return the raw llama_model pointer (needed by LlamaChat for template application). */
    const llama_model* raw_model() const noexcept { return _model; }

    /** Return the raw llama_context pointer. */
    llama_context* raw_ctx() const noexcept { return _ctx; }

    /** Clear the embedding cache (frees memory; next embed() call recomputes). */
    void clear_cache() noexcept { _embedding_cache.clear(); }

private:
    // ── Private helpers ───────────────────────────────────────────────────────

    /**
     * Run the llama.cpp decode loop given a list of pre-formatted tokens.
     *
     * @param tokens      Tokenised prompt.
     * @param temperature Sampling temperature.
     * @param max_tokens  Token generation limit.
     *
     * @returns The decoded response string.
     */
    std::string _run_inference(const std::vector<llama_token>& tokens,
                               float                           temperature,
                               int                             max_tokens) const;

    /**
     * Tokenise a plain UTF-8 string into llama token IDs.
     *
     * @param text      The text to tokenise.
     * @param add_bos   Whether to prepend the BOS token (default true).
     *
     * @returns Vector of token IDs.
     */
    std::vector<llama_token> _tokenize(const std::string& text,
                                       bool               add_bos = true) const;

    /**
     * Compute a raw embedding vector for the given text, bypassing the cache.
     *
     * @param text  The text to embed.
     *
     * @returns Float vector of length llama_n_embd(_model).
     */
    std::vector<float> _compute_embedding(const std::string& text) const;

    // ── Member data ───────────────────────────────────────────────────────────

    llama_model*   _model;            // Owned pointer — freed in destructor
    llama_context* _ctx;              // Owned pointer — freed in destructor
    int            _n_threads;        // Thread count stored for context params
    int            _ctx_size;         // Context window size (shared with LlamaChat)

    // Cache: text → embedding vector (shared across all callers of this model)
    std::unordered_map<std::string, std::vector<float>> _embedding_cache;

    // LlamaChat needs access to _tokenize, _n_threads, and _ctx_size
    friend class LlamaChat;
};

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — stateful multi-turn conversation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Manages a multi-turn conversation with a LlamaModel.
 * Mirrors Python's OllamaChat class.
 *
 * Each LlamaChat instance owns its own history and title, so multiple
 * independent conversations can run simultaneously on the same model.
 *
 * History rollback:
 *   If an inference call fails after the user message was appended, the
 *   message is removed so the history stays consistent.
 *
 * Construction:
 *   LlamaChat conv(model);
 *   LlamaChat conv(model, "You are a helpful assistant.");
 */
class LlamaChat {
public:
    // ── Construction ─────────────────────────────────────────────────────────

    /**
     * Create a new conversation backed by the given model.
     *
     * @param model         A LlamaModel that will process all messages.
     * @param system_prompt The initial system instruction. Must not be blank.
     *
     * @throws std::invalid_argument if system_prompt is blank.
     */
    explicit LlamaChat(LlamaModel&        model,
                       const std::string& system_prompt = "You are a helpful assistant.");

    // Destructor — frees the dedicated chat inference context
    ~LlamaChat() noexcept;

    // Non-copyable — copying a live conversation is almost never correct
    LlamaChat(const LlamaChat&)            = delete;
    LlamaChat& operator=(const LlamaChat&) = delete;

    // Movable — transfers ownership of _chat_ctx and nulls out the source
    LlamaChat(LlamaChat&&) noexcept;
    LlamaChat& operator=(LlamaChat&&) noexcept;

    // ── Conversation ─────────────────────────────────────────────────────────

    /**
     * Send a message and receive a reply, maintaining full conversation history.
     * Mirrors Python's OllamaChat.chat().
     *
     * The message is appended to history before calling the model. If inference
     * fails, the message is rolled back so history stays consistent.
     *
     * On the very first user message, the title is auto-generated asynchronously
     * (a second generate() call condenses the message into a short title).
     *
     * @param message     The user's message text. Must not be blank.
     * @param temperature Sampling temperature in [0.0, 2.0]. Default 0.7.
     * @param max_tokens  Maximum tokens to generate. Must be >= 1. Default 2048.
     *
     * @returns The assistant's reply as a UTF-8 string.
     *
     * @throws std::invalid_argument if message is blank, temperature out of range,
     *                               or max_tokens < 1.
     * @throws std::runtime_error    if inference fails (history is rolled back).
     *
     * Example:
     *   std::string reply = conv.chat("What is 2 + 2?");
     */
    std::string chat(const std::string& message,
                     float              temperature = 0.7f,
                     int                max_tokens  = 2048);

    // ── History management ────────────────────────────────────────────────────

    /**
     * Erase all messages from the conversation history (system prompt retained).
     * Mirrors Python's OllamaChat.clear_history().
     *
     * Example:
     *   conv.clear_history();
     */
    void clear_history() noexcept;

    /**
     * Return a read-only copy of the current conversation history.
     * Mirrors Python's OllamaChat.get_history().
     *
     * The returned vector contains all messages including system, user, and
     * assistant turns in chronological order.
     *
     * @returns A copy of the internal history vector.
     *
     * Example:
     *   for (const auto& msg : conv.get_history()) { ... }
     */
    std::vector<Message> get_history() const;

    // ── Persistence ───────────────────────────────────────────────────────────

    /**
     * Serialise conversation history to a JSON file.
     * Mirrors Python's OllamaChat.save_history().
     *
     * If filepath is empty, a path is generated from the title (or a timestamp
     * if no title is set) in the current working directory.
     *
     * @param filepath  Destination file path. Pass "" to auto-generate.
     *
     * @returns The path where the file was written.
     *
     * @throws std::runtime_error if the file cannot be opened for writing.
     *
     * Example:
     *   std::string path = conv.save_history("my_chat.json");
     */
    std::string save_history(const std::string& filepath = "");

    /**
     * Load conversation history from a JSON file, replacing the current history.
     * Mirrors Python's OllamaChat.load_history().
     *
     * @param filepath  Path to a JSON file previously written by save_history().
     *
     * @throws std::runtime_error    if the file cannot be opened.
     * @throws std::invalid_argument if the JSON structure is malformed or contains
     *                               unknown role strings.
     *
     * Example:
     *   conv.load_history("my_chat.json");
     */
    void load_history(const std::string& filepath);

    // ── Title ─────────────────────────────────────────────────────────────────

    /**
     * Return the current title, or std::nullopt if none has been set/generated.
     * Mirrors Python's OllamaChat.get_title().
     *
     * @returns An optional string.
     *
     * Example:
     *   if (auto t = conv.get_title()) std::cout << *t;
     */
    std::optional<std::string> get_title() const noexcept;

    /**
     * Manually override the conversation title.
     * Mirrors Python's OllamaChat.set_title().
     *
     * @param title  New title. Must not be blank.
     *
     * @throws std::invalid_argument if title is blank.
     *
     * Example:
     *   conv.set_title("My Coding Session");
     */
    void set_title(const std::string& title);

private:
    // ── Private helpers ───────────────────────────────────────────────────────

    /**
     * Ask the model to summarise the first user message into a short title.
     * Called automatically after the first chat() turn.
     *
     * @param first_message The very first user message in this conversation.
     *
     * @returns A short title string (best-effort; falls back gracefully on error).
     */
    std::string _generate_title(const std::string& first_message);

    /**
     * Convert the full history vector into a formatted prompt string suitable
     * for the model, using llama_chat_apply_template.
     *
     * @returns A single string containing the entire formatted prompt.
     */
    std::string _build_prompt() const;

    /**
     * Process only the new tokens (beyond _n_past) into the KV cache and
     * auto-regressively sample the reply.
     *
     * Reuses all previously cached KV vectors — only the suffix not yet seen
     * is fed to llama_decode.  Updates _n_past to include both the new prompt
     * tokens and the generated reply tokens.
     *
     * @param all_tokens  Full tokenised prompt (history + new turn).
     * @param temperature Sampling temperature.
     * @param max_tokens  Maximum tokens to generate.
     *
     * @returns The generated reply string.
     *
     * @throws std::runtime_error if llama_decode fails at any point.
     */
    std::string _run_incremental_inference(const std::vector<llama_token>& all_tokens,
                                           float                           temperature,
                                           int                             max_tokens);

    /**
     * Clear the KV cache of _chat_ctx and reset _n_past to zero.
     * Must be called whenever history is mutated in a way that invalidates
     * previously cached vectors (clear_history, load_history).
     */
    void _reset_kv_cache() noexcept;

    // ── Member data ───────────────────────────────────────────────────────────

    LlamaModel&                  _model;         // Non-owning reference to shared model
    std::string                  _system_prompt; // System instruction (constant after init)
    std::vector<Message>         _history;       // Full conversation: system + user/assistant turns
    std::optional<std::string>   _title;         // Conversation title (set after first message)
    llama_context*               _chat_ctx;      // Dedicated context — owns the KV cache for this conversation
    int32_t                      _n_past;         // Number of tokens already processed into the KV cache
};
