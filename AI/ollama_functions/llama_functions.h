/**
 * llama_functions.h
 * ─────────────────────────────────────────────────────────────────────────────
 * Public API for local AI inference.
 *
 * Previously this file called llama.cpp directly.  It now delegates entirely
 * to AI_convo (../AI_convo/AI_convo.hpp), which is the own-code replacement
 * for the llama library.  No llama.h is included here.
 *
 * Provides:
 *   Role / Message     — conversation primitives (re-exported from AI_convo)
 *   LlamaModel         — stateless inference: generate, embed, similarity, search
 *   LlamaChat          — stateful multi-turn conversation with history & persistence
 *
 * Quick start:
 *   LlamaModel model("path/to/model.gguf");
 *   std::string reply = model.generate("Hello!");
 *
 *   LlamaChat chat(model);
 *   std::string r1 = chat.chat("What is 2 + 2?");
 *   std::string r2 = chat.chat("Why?");           // remembers prior turn
 * ─────────────────────────────────────────────────────────────────────────────
 */

#pragma once

#include "../AI_convo/AI_convo.hpp"

#include <memory>    // std::unique_ptr
#include <optional>
#include <string>
#include <utility>
#include <vector>

// ── Re-export Role / Message / helpers from AI_convo ─────────────────────────
// Existing code that used Role::System, Role::User, etc. continues to compile.

using Role    = ::Role;
using Message = ::Message;

/** Serialize Role to lowercase string ("system" / "user" / "assistant"). */
inline std::string role_to_string(Role r)             { return role_to_str(r); }
/** Parse lowercase string to Role. Throws std::invalid_argument if unknown. */
inline Role        role_from_string(const std::string& s) { return role_from_str(s); }

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — stateless inference, backed by AIModel
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Loads a GGUF model and exposes stateless inference helpers.
 *
 * Internally owns an AIModel.  The AIModel is heap-allocated so its address
 * stays stable when LlamaModel is moved, keeping any associated LlamaChat
 * objects valid.
 *
 * Not copyable.  Movable.
 */
class LlamaModel {
public:
    /**
     * Load a GGUF model from disk.
     *
     * @param model_path  Path to the .gguf file.
     * @param ctx_size    Context window size in tokens (default 4096).
     * @param n_threads   CPU threads for inference (default 4).
     *
     * @throws std::runtime_error on load failure.
     */
    explicit LlamaModel(const std::string& model_path,
                        int ctx_size  = 4096,
                        int n_threads = 4);

    ~LlamaModel() noexcept;

    LlamaModel(const LlamaModel&)            = delete;
    LlamaModel& operator=(const LlamaModel&) = delete;
    LlamaModel(LlamaModel&&) noexcept;
    LlamaModel& operator=(LlamaModel&&) noexcept;

    // ── Inference ─────────────────────────────────────────────────────────────

    /** Generate a single stateless response to a prompt. */
    std::string generate(const std::string& prompt,
                         float temperature = 0.7f,
                         int   max_tokens  = 2048) const;

    /** Embed text into a float vector (cached by default). */
    std::vector<float> embed(const std::string& text, bool use_cache = true);

    /** Cosine similarity between two texts in [-1.0, 1.0]. */
    float similarity(const std::string& text_a, const std::string& text_b);

    /** Semantic search: return top_n (score, label) pairs, highest score first. */
    std::vector<std::pair<float, std::string>>
    search(const std::string&              query,
           const std::vector<std::string>& labels,
           const std::vector<std::string>& texts,
           int top_n = 3);

    /** Discard all cached embedding vectors. */
    void clear_cache() noexcept;

    /** Access the underlying AIModel (needed by LlamaChat). */
    AIModel& ai_model() noexcept;

private:
    std::unique_ptr<AIModel> _impl;   // heap-allocated for stable address

    friend class LlamaChat;
};

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — stateful multi-turn conversation, backed by AIConvo
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Wraps AIConvo to provide a stateful conversation over a LlamaModel.
 *
 * Every call to chat() feeds the complete message history to the model so
 * every reply is always contextually aware of every prior turn.
 *
 * Not copyable.  Movable.
 */
class LlamaChat {
public:
    /**
     * Start a new conversation.
     *
     * @param model         The LlamaModel to use for inference.
     * @param system_prompt Opening instruction for the model.
     *
     * @throws std::invalid_argument if system_prompt is blank.
     */
    explicit LlamaChat(LlamaModel&        model,
                       const std::string& system_prompt = "You are a helpful assistant.");

    ~LlamaChat() noexcept;

    LlamaChat(const LlamaChat&)            = delete;
    LlamaChat& operator=(const LlamaChat&) = delete;
    LlamaChat(LlamaChat&&) noexcept;
    LlamaChat& operator=(LlamaChat&&) noexcept;

    // ── Conversation ──────────────────────────────────────────────────────────

    /** Send a message and get a reply; full history is always included. */
    std::string chat(const std::string& message,
                     float temperature = 0.7f,
                     int   max_tokens  = 2048);

    // ── History ───────────────────────────────────────────────────────────────

    /** Clear all turns; keep the system prompt. */
    void clear_history() noexcept;

    /** Return a copy of the full message history. */
    std::vector<Message> get_history() const;

    // ── Persistence ───────────────────────────────────────────────────────────

    /** Save history to a JSON file.  Empty path auto-generates a filename. */
    std::string save_history(const std::string& filepath = "");

    /** Load history from a JSON file previously written by save_history(). */
    void load_history(const std::string& filepath);

    // ── Title ─────────────────────────────────────────────────────────────────

    /** Return the conversation title, or nullopt if not yet set. */
    std::optional<std::string> get_title() const noexcept;

    /** Override the conversation title manually. */
    void set_title(const std::string& title);

private:
    std::unique_ptr<AIConvo> _impl;   // heap-allocated; owns the conversation state
};
