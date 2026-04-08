/**
 * convo.hpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Lightweight C++17 library for local AI inference built on llama.cpp.
 *
 * Two main classes:
 *   AIModel  — loads a GGUF model; exposes stateless Generate / Embed /
 *              Similarity / Search helpers.
 *   AIConvo  — wraps AIModel with a persistent message history for multi-turn
 *              conversation; includes JSON save/load and auto-title generation.
 *
 * Dependencies:
 *   llama.cpp  (llama.h)
 *   nlohmann/json  (nlohmann/json.hpp)
 *   C++17 standard library
 *
 * Quick start:
 *   AIModel model("my_model.gguf");
 *   std::string answer = model.Generate("What is 42?");
 *
 *   AIConvo convo(model);
 *   std::string r1 = convo.Chat("Hi, who are you?");
 *   std::string r2 = convo.Chat("What did I just ask you?");  // remembers context
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

#include <cstdint>        // uint8_t, int32_t
#include <optional>       // std::optional, std::nullopt
#include <stdexcept>      // std::invalid_argument, std::runtime_error
#include <string>         // std::string
#include <unordered_map>  // std::unordered_map
#include <utility>        // std::pair
#include <vector>         // std::vector

#include <llama.h>           // llama_model, llama_context, llama_token, llama_* API
#include <nlohmann/json.hpp> // nlohmann::json

// ─────────────────────────────────────────────────────────────────────────────
// Role — who authored a message
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Identifies the participant that authored a message in a conversation.
 *
 *   System    — invisible instruction that shapes model behaviour
 *   User      — the human turn
 *   Assistant — the model's reply
 */
enum class Role : uint8_t {
    System,
    User,
    Assistant,
};

/** Serialize a Role to its lowercase string name. */
std::string RoleToStr(Role r);

/** Parse a lowercase string back to a Role.
 *  @throws std::invalid_argument on unknown input. */
Role RoleFromStr(const std::string& s);

// ─────────────────────────────────────────────────────────────────────────────
// Message — one turn in a conversation
// ─────────────────────────────────────────────────────────────────────────────

struct Message {
    Role        role;
    std::string content;
};

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — load a GGUF model; run stateless inference and embeddings
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Owns a llama_model and a default inference context.
 *
 * Multiple AIConvo objects can share a single AIModel because each AIConvo
 * creates its own context with its own KV cache.
 *
 * Not copyable (owns llama pointers). Movable.
 */
class AIModel {
public:
    // ── Construction / destruction ────────────────────────────────────────────

    /**
     * Load a GGUF model from disk and create an inference context.
     *
     * @param model_path   Path to a .gguf model file.
     * @param context_size Context window size in tokens (default 4096).
     * @param thread_count CPU threads for inference (default 4).
     *
     * @throws std::runtime_error on load failure.
     */
    explicit AIModel(const std::string& model_path,
                     int context_size = 4096,
                     int thread_count = 4);

    ~AIModel() noexcept;

    AIModel(const AIModel&)            = delete;
    AIModel& operator=(const AIModel&) = delete;
    AIModel(AIModel&&) noexcept;
    AIModel& operator=(AIModel&&) noexcept;

    // ── Stateless inference ───────────────────────────────────────────────────

    /**
     * Generate a single response to a plain-text prompt.
     * Does not retain any state — each call is fully independent.
     *
     * @param prompt      Text to send to the model.  Must not be blank.
     * @param temperature Sampling temperature in [0.0, 2.0].  Default 0.7.
     * @param max_tokens  Upper bound on tokens to generate.  Must be >= 1.
     *
     * @throws std::invalid_argument on invalid inputs.
     * @throws std::runtime_error    if the model returns nothing.
     */
    std::string Generate(const std::string& prompt,
                         float temperature = 0.7f,
                         int   max_tokens  = 2048) const;

    // ── Embeddings ────────────────────────────────────────────────────────────

    /**
     * Convert text to a dense float embedding vector.
     * Results are cached by text; pass use_cache = false to bypass.
     *
     * @param text       Text to embed.  Must not be blank.
     * @param use_cache  Whether to read/write the internal embedding cache.
     *
     * @throws std::invalid_argument if text is blank.
     * @throws std::runtime_error    if the resulting vector has zero magnitude.
     */
    std::vector<float> Embed(const std::string& text, bool use_cache = true);

    /**
     * Cosine similarity between two pieces of text in [-1.0, 1.0].
     *
     * @throws std::invalid_argument if either text is blank.
     */
    float Similarity(const std::string& a, const std::string& b);

    /**
     * Semantic search: rank candidates by similarity to query.
     *
     * @param query   The search query.
     * @param labels  Display name for each candidate.
     * @param texts   Full text for each candidate (same length as labels).
     * @param top_n   Number of results to return (default 3).
     *
     * @returns Vector of (score, label) pairs, highest score first.
     *
     * @throws std::invalid_argument if query is blank, labels/texts differ in
     *                               size, or top_n < 1.
     */
    std::vector<std::pair<float, std::string>>
    Search(const std::string&               query,
           const std::vector<std::string>&  labels,
           const std::vector<std::string>&  texts,
           int                              top_n = 3);

    // ── Misc ──────────────────────────────────────────────────────────────────

    /** Discard all cached embedding vectors (frees memory). */
    void ClearEmbedCache() noexcept { _embedding_cache.clear(); }

    llama_model*   ModelPtr()    const noexcept { return _model; }
    llama_context* CtxPtr()      const noexcept { return _inference_ctx; }
    int            CtxSize()     const noexcept { return _context_size; }
    int            ThreadCount() const noexcept { return _thread_count; }

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    /** Tokenize text into a vector of token ids. */
    std::vector<llama_token> Tokenize(const std::string& text, bool add_bos = true) const;

    /** Feed tokens into the inference context and sample a reply string. */
    std::string Decode(const std::vector<llama_token>& tokens,
                       float temperature,
                       int   max_tokens) const;

    /** Compute a raw (uncached) embedding for text using a temporary context. */
    std::vector<float> RawEmbed(const std::string& text) const;

    // ── Member data ───────────────────────────────────────────────────────────

    llama_model*   _model;
    llama_context* _inference_ctx;
    int            _context_size;
    int            _thread_count;

    std::unordered_map<std::string, std::vector<float>> _embedding_cache;

    friend class AIConvo;
};

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — stateful multi-turn conversation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Maintains a conversation history and reprocesses the full history on every
 * Chat() turn so the model always has complete context.
 *
 * History includes the initial system prompt plus all user/assistant pairs.
 *
 * Failure safety:
 *   If Chat() throws after the user message was appended, the message is
 *   rolled back and the KV cache is cleared so the next call starts clean.
 *
 * Not copyable (owns a llama_context). Movable.
 */
class AIConvo {
public:
    // ── Construction / destruction ────────────────────────────────────────────

    /**
     * Create a new conversation.
     *
     * @param model         AIModel to use for all inference.
     * @param system_prompt Opening instruction for the model. Must not be blank.
     *
     * @throws std::invalid_argument if system_prompt is blank.
     * @throws std::runtime_error    if the dedicated context cannot be created.
     */
    explicit AIConvo(AIModel&           model,
                     const std::string& system_prompt = "You are a helpful assistant.");

    ~AIConvo() noexcept;

    AIConvo(const AIConvo&)            = delete;
    AIConvo& operator=(const AIConvo&) = delete;
    AIConvo(AIConvo&&) noexcept;
    AIConvo& operator=(AIConvo&&) noexcept;

    // ── Conversation ─────────────────────────────────────────────────────────

    /**
     * Send a message, run inference, and append the reply to history.
     * The full history is always sent so the model sees every prior turn.
     *
     * @param message     User's message text.  Must not be blank.
     * @param temperature Sampling temperature in [0.0, 2.0].
     * @param max_tokens  Max tokens in the reply.  Must be >= 1.
     *
     * @throws std::invalid_argument on invalid inputs.
     * @throws std::runtime_error    on inference failure (history is rolled back).
     */
    std::string Chat(const std::string& message,
                     float temperature = 0.7f,
                     int   max_tokens  = 2048);

    // ── History ───────────────────────────────────────────────────────────────

    /** Clear all user/assistant turns; keep the system prompt. */
    void ClearHistory() noexcept;

    /** Return a copy of the full message history. */
    std::vector<Message> GetHistory() const;

    // ── Persistence ───────────────────────────────────────────────────────────

    /**
     * Write the conversation to a JSON file.
     * If path is empty, a name is derived from the title or current timestamp.
     *
     * @returns The path that was actually written.
     * @throws std::runtime_error if the file cannot be opened or written.
     */
    std::string SaveHistory(const std::string& path = "");

    /**
     * Replace the current history with contents loaded from a JSON file.
     *
     * @throws std::runtime_error    if the file cannot be opened.
     * @throws std::invalid_argument if the JSON is malformed or has unknown roles.
     */
    void LoadHistory(const std::string& path);

    // ── Title ─────────────────────────────────────────────────────────────────

    /** Return the conversation title, or std::nullopt if not yet set. */
    std::optional<std::string> GetTitle() const noexcept;

    /**
     * Override the conversation title manually.
     * @throws std::invalid_argument if title is blank.
     */
    void SetTitle(const std::string& title);

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    /** Format _history into a single prompt string. */
    std::string BuildPrompt() const;

    /**
     * Clear the KV cache and decode the full prompt, then sample the reply.
     * Guarantees the model sees the complete conversation history every turn.
     */
    std::string RunChat(const std::vector<llama_token>& all_tokens,
                        float temperature,
                        int   max_tokens);

    /** Wipe the KV cache of _conversation_ctx and reset _tokens_processed to 0. */
    void ClearKvCache() noexcept;

    /** Generate a short title from the first user message (best-effort). */
    std::string MakeTitle(const std::string& first_msg);

    // ── Member data ───────────────────────────────────────────────────────────

    AIModel&                           _model;
    std::string                        _system_prompt;
    std::vector<Message>               _history;
    std::optional<std::string>         _title;
    llama_context*                     _conversation_ctx;
    int32_t                            _tokens_processed;
};
