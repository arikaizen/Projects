/**
 * llama_functions.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Implementation of llama_functions.h using the llama.cpp C API.
 *
 * Design notes:
 *   - LlamaModel owns llama_model* and llama_context*.  All inference goes
 *     through _run_inference(), which is stateless: it creates a fresh sampler
 *     chain on every call and does not persist KV-cache state between calls.
 *   - LlamaChat builds a full formatted prompt string on every chat() call via
 *     llama_chat_apply_template, so no incremental KV-cache reuse.  This is the
 *     simplest correct approach and mirrors what the Python Ollama client does.
 *   - Embeddings are produced by a second, embedding-only context created with
 *     llama_new_context_with_model (pooling_type = MEAN).
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "llama_functions.h"

// ── Standard library ──────────────────────────────────────────────────────────
#include <algorithm>   // std::sort, std::partial_sort
#include <cassert>     // assert
#include <chrono>      // std::chrono  (timestamp for auto-generated filenames)
#include <cmath>       // std::sqrt, std::fabs
#include <cstring>     // std::memset
#include <fstream>     // std::ofstream, std::ifstream
#include <numeric>     // std::inner_product
#include <sstream>     // std::ostringstream
#include <stdexcept>   // std::runtime_error, std::invalid_argument

// ── nlohmann/json ─────────────────────────────────────────────────────────────
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

/** Trim leading/trailing whitespace from a string. */
static bool is_blank(const std::string& s) {
    // Walk every character; if any non-whitespace found, string is not blank
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/** Compute the L2 (Euclidean) norm of a float vector. */
static float l2_norm(const std::vector<float>& v) {
    // Sum of squares via std::inner_product with itself
    float sum_sq = std::inner_product(v.begin(), v.end(), v.begin(), 0.0f);
    return std::sqrt(sum_sq);
}

/** Cosine similarity of two equal-length float vectors (returns 0 if either is zero). */
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    // Guard: vectors must be the same length
    assert(a.size() == b.size());

    // Compute dot product
    float dot = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);

    // Compute magnitudes
    float norm_a = l2_norm(a);
    float norm_b = l2_norm(b);

    // If either vector is zero-magnitude, similarity is undefined — return 0
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;

    return dot / (norm_a * norm_b);
}

/** Generate an ISO-8601-like timestamp string for use in auto-generated filenames. */
static std::string timestamp_string() {
    // Use system_clock to get a wall-clock time
    auto now   = std::chrono::system_clock::now();
    auto t     = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    // Use thread-safe localtime variant
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    // Format as YYYY-MM-DD_HH-MM-SS
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm_buf);
    return std::string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// Role helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string role_to_string(Role role) {
    switch (role) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
    }
    // Unreachable, but silence compiler warnings
    throw std::invalid_argument("Unknown Role value");
}

Role role_from_string(const std::string& s) {
    if (s == "system")    return Role::System;
    if (s == "user")      return Role::User;
    if (s == "assistant") return Role::Assistant;
    throw std::invalid_argument("Unknown role string: " + s);
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

LlamaModel::LlamaModel(const std::string& model_path, int ctx_size, int n_threads)
    : _model(nullptr), _ctx(nullptr), _n_threads(n_threads), _ctx_size(ctx_size)
{
    // Initialise the llama backend (safe to call multiple times — llama.cpp
    // uses an internal reference count)
    llama_backend_init();

    // ── Load the model from a GGUF file ───────────────────────────────────────

    // Set up model load parameters with sensible defaults
    llama_model_params model_params = llama_model_default_params();
    // model_params.n_gpu_layers can be set here to offload layers to GPU

    // Load the model weights from disk
    _model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!_model) {
        // Model file not found, wrong format, or insufficient RAM
        throw std::runtime_error("Failed to load model: " + model_path);
    }

    // ── Create an inference context ───────────────────────────────────────────

    // Set up context parameters
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx      = ctx_size;   // Maximum tokens in context window
    ctx_params.n_threads  = n_threads;  // Threads for prompt evaluation
    ctx_params.n_threads_batch = n_threads;  // Threads for batch decoding

    // Create the context; this allocates the KV cache
    _ctx = llama_new_context_with_model(_model, ctx_params);
    if (!_ctx) {
        // Free the model before throwing so we don't leak
        llama_model_free(_model);
        _model = nullptr;
        throw std::runtime_error("Failed to create llama context for: " + model_path);
    }
}

LlamaModel::~LlamaModel() noexcept {
    // Free the context first (it holds a reference to the model internally)
    if (_ctx)   { llama_free(_ctx);        _ctx   = nullptr; }
    if (_model) { llama_model_free(_model); _model = nullptr; }
    // Release global backend resources (ref-counted internally by llama.cpp)
    llama_backend_free();
}

LlamaModel::LlamaModel(LlamaModel&& other) noexcept
    : _model(other._model)
    , _ctx(other._ctx)
    , _n_threads(other._n_threads)
    , _embedding_cache(std::move(other._embedding_cache))
{
    // Null out the source so its destructor doesn't double-free
    other._model = nullptr;
    other._ctx   = nullptr;
}

LlamaModel& LlamaModel::operator=(LlamaModel&& other) noexcept {
    if (this != &other) {
        // Free our current resources
        if (_ctx)   { llama_free(_ctx);        }
        if (_model) { llama_model_free(_model); }
        // Steal from source
        _model           = other._model;
        _ctx             = other._ctx;
        _n_threads       = other._n_threads;
        _embedding_cache = std::move(other._embedding_cache);
        // Null out source
        other._model = nullptr;
        other._ctx   = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> LlamaModel::_tokenize(const std::string& text, bool add_bos) const {
    // Ask llama.cpp how many tokens the text will produce (dry run with n=0)
    int n_tokens = llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,   // output buffer = null, size = 0 → count only
        add_bos,      // prepend BOS token?
        false         // special tokens (e.g. control tokens)
    );
    // n_tokens is negative when the buffer is too small — flip it to get the count
    if (n_tokens < 0) n_tokens = -n_tokens;

    // Allocate the token buffer and tokenise for real
    std::vector<llama_token> tokens(n_tokens);
    llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()),
        add_bos, false
    );
    return tokens;
}

std::string LlamaModel::_run_inference(const std::vector<llama_token>& tokens,
                                       float temperature, int max_tokens) const {
    // ── Reset KV-cache state so each generate() is independent ───────────────
    // This means we do not reuse the KV cache across calls, but it keeps the
    // API simple and stateless (mirrors Python ollama.generate behaviour)
    llama_kv_cache_clear(_ctx);

    // ── Evaluate the prompt tokens ────────────────────────────────────────────

    // llama_batch_get_one creates a single-sequence batch from a token array.
    // The third argument is the position offset (0 for fresh contexts).
    llama_batch batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );

    // Decode the prompt — this fills the KV cache for the prompt tokens
    if (llama_decode(_ctx, batch) != 0) {
        throw std::runtime_error("llama_decode failed during prompt evaluation");
    }

    // ── Build a sampler chain ─────────────────────────────────────────────────

    // A sampler chain applies a pipeline of sampling steps (temperature → top-p → etc.)
    llama_sampler_chain_params sampler_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_params);

    // Add temperature sampling — controls randomness
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));

    // Add top-p (nucleus) sampling with p=0.9 to avoid low-probability tokens
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));

    // Add the final distribution sampler (draws one token from the distribution)
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // ── Auto-regressive decoding loop ─────────────────────────────────────────

    std::string result;
    result.reserve(max_tokens * 4);  // Pre-allocate to avoid repeated reallocs

    // Track the current token position (prompt length is the starting offset)
    int32_t n_past = static_cast<int32_t>(tokens.size());

    for (int i = 0; i < max_tokens; ++i) {
        // Sample the next token from the logits in the context
        llama_token new_token = llama_sampler_sample(sampler, _ctx, -1);

        // Check for End-Of-Generation token — model has finished its response
        if (llama_token_is_eog(_model, new_token)) break;

        // ── Convert the token ID to its UTF-8 string piece ────────────────────

        // Most tokens map to 1–4 bytes; 32 bytes is always enough
        char piece[32];
        int piece_len = llama_token_to_piece(_model, new_token, piece, sizeof(piece), 0, false);
        if (piece_len > 0) {
            result.append(piece, piece_len);
        }

        // ── Feed the new token back into the model for the next step ──────────

        // Create a single-token batch at position n_past
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("llama_decode failed during token generation");
        }
        ++n_past;
    }

    // Free the sampler chain (its resources are separate from the context)
    llama_sampler_free(sampler);

    return result;
}

std::vector<float> LlamaModel::_compute_embedding(const std::string& text) const {
    // ── Create a temporary embedding context ──────────────────────────────────
    // We use a fresh context here to avoid mixing inference and embedding state.
    llama_context_params embd_params = llama_context_default_params();
    embd_params.n_ctx            = 512;         // Short context — embeddings don't need much
    embd_params.n_threads        = _n_threads;
    embd_params.embeddings       = true;        // Enable embedding output mode
    embd_params.pooling_type     = LLAMA_POOLING_TYPE_MEAN;  // Mean-pool subword tokens

    llama_context* embd_ctx = llama_new_context_with_model(_model, embd_params);
    if (!embd_ctx) {
        throw std::runtime_error("Failed to create embedding context");
    }

    // ── Tokenise the input text ───────────────────────────────────────────────

    // In embedding mode we do NOT add BOS — it interferes with mean pooling
    std::vector<llama_token> tokens = _tokenize(text, /*add_bos=*/false);

    // ── Decode in embedding mode ──────────────────────────────────────────────

    // llama_batch_get_one: process all tokens as sequence 0
    llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
    if (llama_decode(embd_ctx, batch) != 0) {
        llama_free(embd_ctx);
        throw std::runtime_error("llama_decode failed during embedding computation");
    }

    // ── Extract the pooled embedding vector ───────────────────────────────────

    // llama_get_embeddings_seq returns a pointer to the pooled embedding for sequence 0
    int n_embd = llama_n_embd(_model);
    const float* embd_ptr = llama_get_embeddings_seq(embd_ctx, 0);
    if (!embd_ptr) {
        llama_free(embd_ctx);
        throw std::runtime_error("llama_get_embeddings_seq returned null — "
                                 "ensure the model supports embeddings");
    }

    // Copy the raw float data into a std::vector for safe ownership
    std::vector<float> embedding(embd_ptr, embd_ptr + n_embd);

    llama_free(embd_ctx);
    return embedding;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string LlamaModel::generate(const std::string& prompt,
                                 float              temperature,
                                 int                max_tokens) const {
    // ── Input validation (mirrors Python's ValueError checks) ─────────────────

    if (is_blank(prompt)) {
        throw std::invalid_argument("prompt must not be blank");
    }
    if (temperature < 0.0f || temperature > 2.0f) {
        throw std::invalid_argument(
            "temperature must be in [0.0, 2.0]; got " + std::to_string(temperature));
    }
    if (max_tokens < 1) {
        throw std::invalid_argument(
            "max_tokens must be >= 1; got " + std::to_string(max_tokens));
    }

    // ── Tokenise the prompt ───────────────────────────────────────────────────

    std::vector<llama_token> tokens = _tokenize(prompt, /*add_bos=*/true);

    // ── Run the decoding loop ─────────────────────────────────────────────────

    std::string result = _run_inference(tokens, temperature, max_tokens);

    // ── Validate the response (mirrors Python's RuntimeError check) ───────────

    if (is_blank(result)) {
        throw std::runtime_error("Model returned an empty response");
    }

    return result;
}

std::vector<float> LlamaModel::embed(const std::string& text, bool use_cache) {
    // ── Input validation ──────────────────────────────────────────────────────

    if (is_blank(text)) {
        throw std::invalid_argument("text must not be blank");
    }

    // ── Cache lookup ──────────────────────────────────────────────────────────

    if (use_cache) {
        auto it = _embedding_cache.find(text);
        if (it != _embedding_cache.end()) {
            // Return cached result without calling the model
            return it->second;
        }
    }

    // ── Compute embedding ─────────────────────────────────────────────────────

    std::vector<float> embedding = _compute_embedding(text);

    // ── Guard against zero-magnitude vectors (would cause NaN in similarity) ──

    if (l2_norm(embedding) == 0.0f) {
        throw std::runtime_error(
            "Embedding vector has zero magnitude — the model may not support embeddings");
    }

    // ── Store in cache for future calls ───────────────────────────────────────

    if (use_cache) {
        _embedding_cache[text] = embedding;
    }

    return embedding;
}

float LlamaModel::similarity(const std::string& text_a, const std::string& text_b) {
    // ── Input validation ──────────────────────────────────────────────────────

    if (is_blank(text_a)) throw std::invalid_argument("text_a must not be blank");
    if (is_blank(text_b)) throw std::invalid_argument("text_b must not be blank");

    // ── Embed both texts (uses cache automatically) ───────────────────────────

    std::vector<float> vec_a = embed(text_a);
    std::vector<float> vec_b = embed(text_b);

    // ── Compute cosine similarity ─────────────────────────────────────────────

    return cosine_similarity(vec_a, vec_b);
}

std::vector<std::pair<float, std::string>>
LlamaModel::search(const std::string&              query,
                   const std::vector<std::string>& labels,
                   const std::vector<std::string>& texts,
                   int                             top_n) {
    // ── Input validation ──────────────────────────────────────────────────────

    if (is_blank(query)) {
        throw std::invalid_argument("query must not be blank");
    }
    if (labels.size() != texts.size()) {
        throw std::invalid_argument(
            "labels and texts must have the same length; got "
            + std::to_string(labels.size()) + " vs " + std::to_string(texts.size()));
    }
    if (top_n < 1) {
        throw std::invalid_argument("top_n must be >= 1; got " + std::to_string(top_n));
    }

    // ── Embed the query ───────────────────────────────────────────────────────

    std::vector<float> query_vec = embed(query);

    // ── Score each candidate ──────────────────────────────────────────────────

    // Build a list of (score, label) pairs
    std::vector<std::pair<float, std::string>> scored;
    scored.reserve(texts.size());

    for (std::size_t i = 0; i < texts.size(); ++i) {
        // Embed each candidate text (uses cache if available)
        std::vector<float> candidate_vec = embed(texts[i]);
        float score = cosine_similarity(query_vec, candidate_vec);
        scored.emplace_back(score, labels[i]);
    }

    // ── Sort by score descending ──────────────────────────────────────────────

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // ── Return the top_n results ──────────────────────────────────────────────

    // Clamp top_n to the number of candidates available
    int actual_n = std::min(top_n, static_cast<int>(scored.size()));
    return std::vector<std::pair<float, std::string>>(
        scored.begin(), scored.begin() + actual_n);
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — construction
// ─────────────────────────────────────────────────────────────────────────────

LlamaChat::LlamaChat(LlamaModel& model, const std::string& system_prompt)
    : _model(model), _system_prompt(system_prompt), _chat_ctx(nullptr), _n_past(0)
{
    // ── Validate the system prompt ────────────────────────────────────────────

    if (is_blank(system_prompt)) {
        throw std::invalid_argument("system_prompt must not be blank");
    }

    // ── Create a dedicated inference context for this conversation ────────────
    // Each LlamaChat gets its own llama_context so its KV cache is completely
    // independent from other chats and from LlamaModel::generate().
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx         = model._ctx_size;   // Match model's configured window size
    ctx_params.n_threads     = model._n_threads;
    ctx_params.n_threads_batch = model._n_threads;

    _chat_ctx = llama_new_context_with_model(model.raw_model(), ctx_params);
    if (!_chat_ctx) {
        throw std::runtime_error("Failed to create dedicated chat context");
    }

    // ── Seed the history with the system message ──────────────────────────────
    // The system message is always first in the history, just like in the Python
    // implementation where SYSTEM_PROMPT is inserted at construction time.
    _history.push_back({Role::System, system_prompt});
}

LlamaChat::~LlamaChat() noexcept {
    // Free our dedicated KV-cache context (model weights are NOT owned here)
    if (_chat_ctx) {
        llama_free(_chat_ctx);
        _chat_ctx = nullptr;
    }
}

LlamaChat::LlamaChat(LlamaChat&& other) noexcept
    : _model(other._model)
    , _system_prompt(std::move(other._system_prompt))
    , _history(std::move(other._history))
    , _title(std::move(other._title))
    , _chat_ctx(other._chat_ctx)
    , _n_past(other._n_past)
{
    // Null out the source so its destructor doesn't free the context we just took
    other._chat_ctx = nullptr;
    other._n_past   = 0;
}

LlamaChat& LlamaChat::operator=(LlamaChat&& other) noexcept {
    if (this != &other) {
        // Free our current context before stealing the other's
        if (_chat_ctx) llama_free(_chat_ctx);

        // _model is a reference — cannot be rebound, so move-assign only works
        // when both sides reference the same model (enforced by design)
        _system_prompt = std::move(other._system_prompt);
        _history       = std::move(other._history);
        _title         = std::move(other._title);
        _chat_ctx      = other._chat_ctx;
        _n_past        = other._n_past;

        other._chat_ctx = nullptr;
        other._n_past   = 0;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — private helpers
// ─────────────────────────────────────────────────────────────────────────────

void LlamaChat::_reset_kv_cache() noexcept {
    // Wipe every KV vector stored in this conversation's context
    llama_kv_cache_clear(_chat_ctx);
    // Reset the cursor — next chat() will re-process from token 0
    _n_past = 0;
}

std::string LlamaChat::_run_incremental_inference(
    const std::vector<llama_token>& all_tokens,
    float temperature, int max_tokens)
{
    // ── Compute how many new tokens the model hasn't seen yet ─────────────────
    int32_t n_new = static_cast<int32_t>(all_tokens.size()) - _n_past;

    // Safety: if _n_past somehow got ahead (e.g. after a partial failure),
    // fall back to a full reprocess rather than feeding garbage offsets
    if (n_new <= 0) {
        _reset_kv_cache();
        n_new = static_cast<int32_t>(all_tokens.size());
    }

    // ── Feed only the new suffix tokens into the KV cache ────────────────────
    // all_tokens[0.._n_past) are already cached — skip them entirely
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(all_tokens.data()) + _n_past,
        n_new
    );
    if (llama_decode(_chat_ctx, prompt_batch) != 0) {
        throw std::runtime_error("llama_decode failed during prompt evaluation");
    }
    // Advance the cursor past the new prompt tokens we just processed
    _n_past += n_new;

    // ── Build a sampler chain ─────────────────────────────────────────────────
    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // ── Auto-regressive decoding loop ─────────────────────────────────────────
    std::string result;
    result.reserve(max_tokens * 4);

    for (int i = 0; i < max_tokens; ++i) {
        // Sample one token from the current logits
        llama_token new_token = llama_sampler_sample(sampler, _chat_ctx, -1);

        // End-of-generation token means the model is done
        if (llama_token_is_eog(_model.raw_model(), new_token)) break;

        // Convert the token ID to its UTF-8 string piece
        char piece[32];
        int piece_len = llama_token_to_piece(
            _model.raw_model(), new_token, piece, sizeof(piece), 0, false);
        if (piece_len > 0) result.append(piece, piece_len);

        // Feed the generated token back so it becomes part of the KV cache
        // for the next iteration — this is what makes generation auto-regressive
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(_chat_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("llama_decode failed during token generation");
        }
        // Each generated token extends the cached sequence by one position
        ++_n_past;
    }

    llama_sampler_free(sampler);
    return result;
}

std::string LlamaChat::_build_prompt() const {
    // ── Convert our Message vector to the llama_chat_message C struct array ───

    // llama_chat_apply_template expects a C array of llama_chat_message structs
    // (each has const char* role and const char* content)
    std::vector<llama_chat_message> chat_msgs;
    chat_msgs.reserve(_history.size());

    // Store the role strings to keep them alive while building the prompt
    std::vector<std::string> role_strings;
    role_strings.reserve(_history.size());

    for (const auto& msg : _history) {
        role_strings.push_back(role_to_string(msg.role));
        chat_msgs.push_back({role_strings.back().c_str(), msg.content.c_str()});
    }

    // ── Apply the model's chat template ───────────────────────────────────────

    // Dry run: pass null buffer to get the required buffer size
    int required = llama_chat_apply_template(
        _model.raw_model(),
        nullptr,           // use the model's built-in template
        chat_msgs.data(),
        static_cast<int>(chat_msgs.size()),
        true,              // add_ass: append the assistant prompt prefix
        nullptr, 0         // null buffer → count only
    );

    if (required < 0) {
        // Model has no built-in template — fall back to a simple concatenation
        std::ostringstream oss;
        for (const auto& msg : _history) {
            oss << role_to_string(msg.role) << ": " << msg.content << "\n";
        }
        oss << "assistant: ";
        return oss.str();
    }

    // Allocate a buffer large enough for the formatted prompt
    std::vector<char> buf(required + 1, '\0');
    llama_chat_apply_template(
        _model.raw_model(),
        nullptr,
        chat_msgs.data(),
        static_cast<int>(chat_msgs.size()),
        true,
        buf.data(), static_cast<int>(buf.size())
    );

    return std::string(buf.data(), required);
}

std::string LlamaChat::_generate_title(const std::string& first_message) {
    // Ask the model to condense the first user message into a short title.
    // We use generate() so it does not affect our conversation history.
    std::string title_prompt =
        "In 5 words or fewer, summarise this message as a short conversation title.\n"
        "Message: " + first_message + "\n"
        "Title:";

    try {
        // Generate with low temperature for a deterministic, concise title
        std::string raw = _model.generate(title_prompt, /*temperature=*/0.3f, /*max_tokens=*/20);

        // Strip leading/trailing whitespace from the generated title
        auto start = raw.find_first_not_of(" \t\n\r");
        auto end   = raw.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return first_message.substr(0, 40);
        return raw.substr(start, end - start + 1);
    } catch (const std::exception&) {
        // Title generation is best-effort — fall back to a truncated first message
        return first_message.substr(0, std::min<std::size_t>(40, first_message.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string LlamaChat::chat(const std::string& message,
                            float              temperature,
                            int                max_tokens) {
    // ── Input validation (mirrors Python's ValueError checks) ─────────────────

    if (is_blank(message)) {
        throw std::invalid_argument("message must not be blank");
    }
    if (temperature < 0.0f || temperature > 2.0f) {
        throw std::invalid_argument(
            "temperature must be in [0.0, 2.0]; got " + std::to_string(temperature));
    }
    if (max_tokens < 1) {
        throw std::invalid_argument(
            "max_tokens must be >= 1; got " + std::to_string(max_tokens));
    }

    // ── Check whether this is the first user message ──────────────────────────

    // Count user messages already in history (excluding system prompt)
    bool is_first_user_message = true;
    for (const auto& msg : _history) {
        if (msg.role == Role::User) { is_first_user_message = false; break; }
    }

    // ── Append the user message to history (optimistically) ──────────────────

    // We append before calling the model so the history is correct when we format
    // the prompt.  If inference fails we roll back this append.
    _history.push_back({Role::User, message});

    try {
        // ── Format the full conversation as a prompt string ───────────────────

        std::string prompt = _build_prompt();

        // ── Tokenise the full prompt ──────────────────────────────────────────

        std::vector<llama_token> tokens = _model._tokenize(prompt, /*add_bos=*/true);

        // ── Run incremental inference (reuses cached KV vectors) ──────────────
        // Only tokens beyond _n_past are fed to llama_decode — the rest are
        // already in the KV cache from previous turns.
        std::string reply = _run_incremental_inference(tokens, temperature, max_tokens);

        if (is_blank(reply)) {
            throw std::runtime_error("Model returned an empty response");
        }

        // ── Append the assistant reply to history ─────────────────────────────

        _history.push_back({Role::Assistant, reply});

        // ── Auto-generate a title from the first user message ─────────────────

        if (is_first_user_message && !_title.has_value()) {
            // Best-effort — any exception here must not surface to the caller
            try {
                _title = _generate_title(message);
            } catch (...) {
                // Title is optional — silently swallow any error
            }
        }

        return reply;

    } catch (...) {
        // ── Rollback on failure ───────────────────────────────────────────────
        // Remove the user message we optimistically appended so history stays
        // consistent, mirroring Python's rollback logic.
        _history.pop_back();
        // Also wipe the KV cache — it may be partially updated and out of sync
        // with the history we just rolled back.  Next call reprocesses cleanly.
        _reset_kv_cache();
        throw;  // Re-raise the original exception unchanged
    }
}

void LlamaChat::clear_history() noexcept {
    // Remove all messages except the initial system prompt (always index 0)
    _history.erase(_history.begin() + 1, _history.end());
    // Wipe the KV cache — the cached vectors represent the old turns which are
    // now gone.  Next chat() will rebuild from just the system prompt.
    _reset_kv_cache();
    // Note: title is intentionally kept — clearing history doesn't rename the chat
}

std::vector<Message> LlamaChat::get_history() const {
    // Return a copy so callers cannot mutate the internal state
    return _history;
}

std::string LlamaChat::save_history(const std::string& filepath) {
    // ── Build the JSON document ───────────────────────────────────────────────

    json doc;

    // Serialise the title (null if not set)
    doc["title"] = _title.has_value() ? json(*_title) : json(nullptr);

    // Serialise each message as {"role": "...", "content": "..."}
    json messages = json::array();
    for (const auto& msg : _history) {
        messages.push_back({
            {"role",    role_to_string(msg.role)},
            {"content", msg.content}
        });
    }
    doc["messages"] = messages;

    // ── Determine output path ─────────────────────────────────────────────────

    std::string output_path = filepath;
    if (is_blank(output_path)) {
        // Auto-generate a filename from the title or current timestamp
        std::string base;
        if (_title.has_value()) {
            // Sanitise title: replace spaces/slashes with underscores
            base = *_title;
            for (char& c : base) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
        } else {
            base = "chat_" + timestamp_string();
        }
        output_path = base + ".json";
    }

    // ── Write JSON to disk ────────────────────────────────────────────────────

    std::ofstream ofs(output_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    }
    ofs << doc.dump(2);  // Pretty-print with 2-space indentation
    if (!ofs) {
        throw std::runtime_error("Write failed for: " + output_path);
    }

    return output_path;
}

void LlamaChat::load_history(const std::string& filepath) {
    // ── Open and parse the JSON file ──────────────────────────────────────────

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open file for reading: " + filepath);
    }

    json doc;
    try {
        ifs >> doc;  // Parse JSON from the file stream
    } catch (const json::parse_error& e) {
        throw std::invalid_argument(
            std::string("Malformed JSON in ") + filepath + ": " + e.what());
    }

    // ── Validate top-level structure ──────────────────────────────────────────

    if (!doc.is_object()) {
        throw std::invalid_argument("Expected a JSON object at the top level");
    }
    if (!doc.contains("messages") || !doc["messages"].is_array()) {
        throw std::invalid_argument("JSON must contain a 'messages' array");
    }

    // ── Parse the messages array ──────────────────────────────────────────────

    std::vector<Message> new_history;
    for (const auto& item : doc["messages"]) {
        // Each item must have string "role" and string "content"
        if (!item.contains("role") || !item["role"].is_string()) {
            throw std::invalid_argument("Each message must have a string 'role' field");
        }
        if (!item.contains("content") || !item["content"].is_string()) {
            throw std::invalid_argument("Each message must have a string 'content' field");
        }

        // role_from_string throws std::invalid_argument for unknown roles
        Role role = role_from_string(item["role"].get<std::string>());
        new_history.push_back({role, item["content"].get<std::string>()});
    }

    // ── Parse the optional title ──────────────────────────────────────────────

    std::optional<std::string> new_title;
    if (doc.contains("title") && doc["title"].is_string()) {
        new_title = doc["title"].get<std::string>();
    }

    // ── Commit — only update state after all validation passes ────────────────

    _history = std::move(new_history);
    _title   = std::move(new_title);
    // Wipe the KV cache — the loaded history has never been through llama_decode,
    // so there are no cached vectors for it.  Next chat() reprocesses from scratch.
    _reset_kv_cache();
}

std::optional<std::string> LlamaChat::get_title() const noexcept {
    return _title;
}

void LlamaChat::set_title(const std::string& title) {
    if (is_blank(title)) {
        throw std::invalid_argument("title must not be blank");
    }
    _title = title;
}
