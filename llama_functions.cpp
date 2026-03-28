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
 *     llama_init_from_model (pooling_type = MEAN).
 *
 * API compatibility: built against llama.cpp >= b3000 (post-Makefile removal).
 *   Key changes from older llama.cpp:
 *     - llama_new_context_with_model  → llama_init_from_model
 *     - llama_tokenize / token_to_piece / token_is_eog now take const llama_vocab*
 *       (obtained via llama_model_get_vocab)
 *     - llama_kv_cache_clear          → llama_kv_self_clear
 *     - llama_n_embd                  → llama_model_n_embd
 *     - llama_chat_apply_template: first arg is now const char* (template string),
 *       not a model pointer; get it with llama_model_chat_template()
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

/** Return true if every character in s is whitespace (or s is empty). */
static bool is_blank(const std::string& s) {
    for (char c : s) {
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

/** Compute the L2 (Euclidean) norm of a float vector. */
static float l2_norm(const std::vector<float>& v) {
    float sum_sq = std::inner_product(v.begin(), v.end(), v.begin(), 0.0f);
    return std::sqrt(sum_sq);
}

/** Cosine similarity of two equal-length float vectors (returns 0 if either is zero). */
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    assert(a.size() == b.size());
    float dot    = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
    float norm_a = l2_norm(a);
    float norm_b = l2_norm(b);
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (norm_a * norm_b);
}

/** Generate an ISO-8601-like timestamp string (YYYY-MM-DD_HH-MM-SS). */
static std::string timestamp_string() {
    auto now   = std::chrono::system_clock::now();
    auto t     = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
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
    // Initialise the llama backend (ref-counted internally — safe to call many times)
    llama_backend_init();

    // ── Load the model from a GGUF file ───────────────────────────────────────
    llama_model_params model_params = llama_model_default_params();
    // model_params.n_gpu_layers = N;  // uncomment to offload N layers to GPU

    _model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!_model) {
        throw std::runtime_error("Failed to load model: " + model_path);
    }

    // ── Create an inference context ───────────────────────────────────────────
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx           = ctx_size;   // Maximum tokens in context window
    ctx_params.n_threads       = n_threads;  // Threads for prompt evaluation
    ctx_params.n_threads_batch = n_threads;  // Threads for batch decoding

    // NOTE: llama_new_context_with_model was renamed to llama_init_from_model
    //       in llama.cpp >= b3000.
    _ctx = llama_init_from_model(_model, ctx_params);
    if (!_ctx) {
        llama_model_free(_model);
        _model = nullptr;
        throw std::runtime_error("Failed to create llama context for: " + model_path);
    }
}

LlamaModel::~LlamaModel() noexcept {
    // Free context first — it holds an internal reference to the model
    if (_ctx)   { llama_free(_ctx);         _ctx   = nullptr; }
    if (_model) { llama_model_free(_model); _model = nullptr; }
    // Release global backend resources (ref-counted inside llama.cpp)
    llama_backend_free();
}

LlamaModel::LlamaModel(LlamaModel&& other) noexcept
    : _model(other._model)
    , _ctx(other._ctx)
    , _n_threads(other._n_threads)
    , _ctx_size(other._ctx_size)
    , _embedding_cache(std::move(other._embedding_cache))
{
    // Null out the source so its destructor does not double-free
    other._model = nullptr;
    other._ctx   = nullptr;
}

LlamaModel& LlamaModel::operator=(LlamaModel&& other) noexcept {
    if (this != &other) {
        if (_ctx)   { llama_free(_ctx);         }
        if (_model) { llama_model_free(_model); }
        _model           = other._model;
        _ctx             = other._ctx;
        _n_threads       = other._n_threads;
        _ctx_size        = other._ctx_size;
        _embedding_cache = std::move(other._embedding_cache);
        other._model     = nullptr;
        other._ctx       = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel — private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> LlamaModel::_tokenize(const std::string& text, bool add_bos) const {
    // NOTE: llama_tokenize now takes const llama_vocab* instead of llama_model*.
    //       Obtain the vocab pointer from the model via llama_model_get_vocab().
    const llama_vocab* vocab = llama_model_get_vocab(_model);

    // Dry run with null buffer to find the required token count
    int n_tokens = llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,   // output buffer = null, size = 0 → count only
        add_bos,      // prepend BOS token?
        false         // parse special control tokens?
    );
    // A negative return means the buffer was too small — negate to get the count
    if (n_tokens < 0) n_tokens = -n_tokens;

    // Allocate the token buffer and tokenise for real
    std::vector<llama_token> tokens(n_tokens);
    llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()),
        add_bos, false
    );
    return tokens;
}

std::string LlamaModel::_run_inference(const std::vector<llama_token>& tokens,
                                       float temperature, int max_tokens) const {
    // NOTE: llama_kv_cache_clear was renamed to llama_kv_self_clear.
    //       Reset KV state so each generate() call is independent.
    llama_kv_self_clear(_ctx);

    // Evaluate the prompt tokens in one batch
    llama_batch batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );
    if (llama_decode(_ctx, batch) != 0) {
        throw std::runtime_error("llama_decode failed during prompt evaluation");
    }

    // ── Build a sampler chain ─────────────────────────────────────────────────
    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // NOTE: vocab pointer needed for token_is_eog and token_to_piece (API change)
    const llama_vocab* vocab = llama_model_get_vocab(_model);

    // ── Auto-regressive decoding loop ─────────────────────────────────────────
    std::string result;
    result.reserve(max_tokens * 4);

    for (int i = 0; i < max_tokens; ++i) {
        llama_token new_token = llama_sampler_sample(sampler, _ctx, -1);

        // NOTE: llama_token_is_eog now takes const llama_vocab* not llama_model*
        if (llama_vocab_is_eog(vocab, new_token)) break;

        // NOTE: llama_token_to_piece now takes const llama_vocab* not llama_model*
        char piece[32];
        int piece_len = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, false);
        if (piece_len > 0) result.append(piece, piece_len);

        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("llama_decode failed during token generation");
        }
    }

    llama_sampler_free(sampler);
    return result;
}

std::vector<float> LlamaModel::_compute_embedding(const std::string& text) const {
    // ── Create a temporary embedding context ──────────────────────────────────
    llama_context_params embd_params = llama_context_default_params();
    embd_params.n_ctx        = 512;
    embd_params.n_threads    = _n_threads;
    embd_params.embeddings   = true;
    embd_params.pooling_type = LLAMA_POOLING_TYPE_MEAN;

    // NOTE: llama_new_context_with_model renamed to llama_init_from_model
    llama_context* embd_ctx = llama_init_from_model(_model, embd_params);
    if (!embd_ctx) {
        throw std::runtime_error("Failed to create embedding context");
    }

    // Tokenise without BOS — BOS interferes with mean pooling
    std::vector<llama_token> tokens = _tokenize(text, /*add_bos=*/false);

    llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
    if (llama_decode(embd_ctx, batch) != 0) {
        llama_free(embd_ctx);
        throw std::runtime_error("llama_decode failed during embedding computation");
    }

    // NOTE: llama_n_embd(_model) renamed to llama_model_n_embd(_model)
    int n_embd = llama_model_n_embd(_model);

    // llama_get_embeddings_seq returns the mean-pooled vector for sequence 0
    const float* embd_ptr = llama_get_embeddings_seq(embd_ctx, 0);
    if (!embd_ptr) {
        llama_free(embd_ctx);
        throw std::runtime_error("llama_get_embeddings_seq returned null — "
                                 "ensure the model supports embeddings");
    }

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
    if (is_blank(prompt))
        throw std::invalid_argument("prompt must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "temperature must be in [0.0, 2.0]; got " + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "max_tokens must be >= 1; got " + std::to_string(max_tokens));

    std::vector<llama_token> tokens = _tokenize(prompt, /*add_bos=*/true);
    std::string result = _run_inference(tokens, temperature, max_tokens);

    if (is_blank(result))
        throw std::runtime_error("Model returned an empty response");

    return result;
}

std::vector<float> LlamaModel::embed(const std::string& text, bool use_cache) {
    if (is_blank(text))
        throw std::invalid_argument("text must not be blank");

    if (use_cache) {
        auto it = _embedding_cache.find(text);
        if (it != _embedding_cache.end()) return it->second;
    }

    std::vector<float> embedding = _compute_embedding(text);

    if (l2_norm(embedding) == 0.0f)
        throw std::runtime_error(
            "Embedding vector has zero magnitude — the model may not support embeddings");

    if (use_cache) _embedding_cache[text] = embedding;
    return embedding;
}

float LlamaModel::similarity(const std::string& text_a, const std::string& text_b) {
    if (is_blank(text_a)) throw std::invalid_argument("text_a must not be blank");
    if (is_blank(text_b)) throw std::invalid_argument("text_b must not be blank");
    return cosine_similarity(embed(text_a), embed(text_b));
}

std::vector<std::pair<float, std::string>>
LlamaModel::search(const std::string&              query,
                   const std::vector<std::string>& labels,
                   const std::vector<std::string>& texts,
                   int                             top_n) {
    if (is_blank(query))
        throw std::invalid_argument("query must not be blank");
    if (labels.size() != texts.size())
        throw std::invalid_argument(
            "labels and texts must have the same length; got "
            + std::to_string(labels.size()) + " vs " + std::to_string(texts.size()));
    if (top_n < 1)
        throw std::invalid_argument("top_n must be >= 1; got " + std::to_string(top_n));

    std::vector<float> query_vec = embed(query);

    std::vector<std::pair<float, std::string>> scored;
    scored.reserve(texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i) {
        float score = cosine_similarity(query_vec, embed(texts[i]));
        scored.emplace_back(score, labels[i]);
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    int actual_n = std::min(top_n, static_cast<int>(scored.size()));
    return {scored.begin(), scored.begin() + actual_n};
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — construction
// ─────────────────────────────────────────────────────────────────────────────

LlamaChat::LlamaChat(LlamaModel& model, const std::string& system_prompt)
    : _model(model), _system_prompt(system_prompt), _chat_ctx(nullptr), _n_past(0)
{
    if (is_blank(system_prompt))
        throw std::invalid_argument("system_prompt must not be blank");

    // ── Create a dedicated inference context for this conversation ────────────
    // Each LlamaChat gets its own llama_context so its KV cache is independent
    // from LlamaModel::generate() and from other LlamaChat instances.
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx           = model._ctx_size;
    ctx_params.n_threads       = model._n_threads;
    ctx_params.n_threads_batch = model._n_threads;

    // NOTE: use model._model (llama_model*, non-const) — LlamaChat is a friend
    //       of LlamaModel so it can access the private member directly.
    //       raw_model() returns const llama_model* which llama_init_from_model
    //       does not accept.
    _chat_ctx = llama_init_from_model(model._model, ctx_params);
    if (!_chat_ctx)
        throw std::runtime_error("Failed to create dedicated chat context");

    // Seed history with the system message (always index 0)
    _history.push_back({Role::System, system_prompt});
}

LlamaChat::~LlamaChat() noexcept {
    // Model weights are NOT owned here — only the KV-cache context is freed
    if (_chat_ctx) { llama_free(_chat_ctx); _chat_ctx = nullptr; }
}

LlamaChat::LlamaChat(LlamaChat&& other) noexcept
    : _model(other._model)
    , _system_prompt(std::move(other._system_prompt))
    , _history(std::move(other._history))
    , _title(std::move(other._title))
    , _chat_ctx(other._chat_ctx)
    , _n_past(other._n_past)
{
    other._chat_ctx = nullptr;
    other._n_past   = 0;
}

LlamaChat& LlamaChat::operator=(LlamaChat&& other) noexcept {
    if (this != &other) {
        if (_chat_ctx) llama_free(_chat_ctx);
        // _model is a reference — both sides must reference the same LlamaModel
        _system_prompt  = std::move(other._system_prompt);
        _history        = std::move(other._history);
        _title          = std::move(other._title);
        _chat_ctx       = other._chat_ctx;
        _n_past         = other._n_past;
        other._chat_ctx = nullptr;
        other._n_past   = 0;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — private helpers
// ─────────────────────────────────────────────────────────────────────────────

void LlamaChat::_reset_kv_cache() noexcept {
    // NOTE: llama_kv_cache_clear renamed to llama_kv_self_clear
    llama_kv_self_clear(_chat_ctx);
    _n_past = 0;  // Next chat() will re-process history from token 0
}

std::string LlamaChat::_run_incremental_inference(
    const std::vector<llama_token>& all_tokens,
    float temperature, int max_tokens)
{
    // Compute how many new tokens the model hasn't seen yet
    int32_t n_new = static_cast<int32_t>(all_tokens.size()) - _n_past;

    // Guard: if _n_past somehow got ahead, fall back to full reprocess
    if (n_new <= 0) {
        _reset_kv_cache();
        n_new = static_cast<int32_t>(all_tokens.size());
    }

    // Feed only the new suffix tokens — all_tokens[0.._n_past) are already cached
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(all_tokens.data()) + _n_past,
        n_new
    );
    if (llama_decode(_chat_ctx, prompt_batch) != 0)
        throw std::runtime_error("llama_decode failed during prompt evaluation");
    _n_past += n_new;

    // ── Build a sampler chain ─────────────────────────────────────────────────
    llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // NOTE: vocab pointer required for token_is_eog and token_to_piece
    const llama_vocab* vocab = llama_model_get_vocab(_model._model);

    // ── Auto-regressive decoding loop ─────────────────────────────────────────
    std::string result;
    result.reserve(max_tokens * 4);

    for (int i = 0; i < max_tokens; ++i) {
        llama_token new_token = llama_sampler_sample(sampler, _chat_ctx, -1);

        // NOTE: llama_vocab_is_eog replaces llama_token_is_eog
        if (llama_vocab_is_eog(vocab, new_token)) break;

        // NOTE: llama_token_to_piece now takes const llama_vocab*
        char piece[32];
        int piece_len = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, false);
        if (piece_len > 0) result.append(piece, piece_len);

        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(_chat_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("llama_decode failed during token generation");
        }
        ++_n_past;  // Each generated token extends the cached sequence
    }

    llama_sampler_free(sampler);
    return result;
}

std::string LlamaChat::_build_prompt() const {
    // Convert our Message vector to the llama_chat_message C struct array
    std::vector<llama_chat_message> chat_msgs;
    chat_msgs.reserve(_history.size());

    // Keep role strings alive for the duration of this function
    std::vector<std::string> role_strings;
    role_strings.reserve(_history.size());

    for (const auto& msg : _history) {
        role_strings.push_back(role_to_string(msg.role));
        chat_msgs.push_back({role_strings.back().c_str(), msg.content.c_str()});
    }

    // NOTE: llama_chat_apply_template API changed — the first argument is now
    //       const char* (the template string itself), NOT a llama_model pointer.
    //       Retrieve the model's built-in template via llama_model_chat_template().
    //       Pass nullptr as the name to get the default template for this model.
    const char* tmpl = llama_model_chat_template(_model._model, /*name=*/nullptr);

    // Dry run: pass null buffer to get the required buffer size
    int required = llama_chat_apply_template(
        tmpl,
        chat_msgs.data(),
        static_cast<int>(chat_msgs.size()),
        /*add_ass=*/true,
        nullptr, 0  // null buffer → count only
    );

    if (required < 0) {
        // Model has no built-in template — fall back to plain concatenation
        std::ostringstream oss;
        for (const auto& msg : _history) {
            oss << role_to_string(msg.role) << ": " << msg.content << "\n";
        }
        oss << "assistant: ";
        return oss.str();
    }

    // Allocate exactly the required buffer and apply the template for real
    std::vector<char> buf(required + 1, '\0');
    llama_chat_apply_template(
        tmpl,
        chat_msgs.data(),
        static_cast<int>(chat_msgs.size()),
        /*add_ass=*/true,
        buf.data(), static_cast<int>(buf.size())
    );

    return std::string(buf.data(), required);
}

std::string LlamaChat::_generate_title(const std::string& first_message) {
    // Ask the model for a concise title without touching conversation history
    std::string title_prompt =
        "In 5 words or fewer, summarise this message as a short conversation title.\n"
        "Message: " + first_message + "\n"
        "Title:";

    try {
        std::string raw = _model.generate(title_prompt, /*temperature=*/0.3f, /*max_tokens=*/20);
        auto start = raw.find_first_not_of(" \t\n\r");
        auto end   = raw.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return first_message.substr(0, 40);
        return raw.substr(start, end - start + 1);
    } catch (const std::exception&) {
        // Title generation is best-effort — fall back silently
        return first_message.substr(0, std::min<std::size_t>(40, first_message.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string LlamaChat::chat(const std::string& message,
                            float              temperature,
                            int                max_tokens) {
    if (is_blank(message))
        throw std::invalid_argument("message must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "temperature must be in [0.0, 2.0]; got " + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "max_tokens must be >= 1; got " + std::to_string(max_tokens));

    // Remember whether this is the first user message (for title generation)
    bool is_first_user_message = true;
    for (const auto& msg : _history) {
        if (msg.role == Role::User) { is_first_user_message = false; break; }
    }

    // Append the user message before building the prompt (optimistic append)
    _history.push_back({Role::User, message});

    try {
        std::string              prompt = _build_prompt();
        std::vector<llama_token> tokens = _model._tokenize(prompt, /*add_bos=*/true);
        std::string              reply  = _run_incremental_inference(tokens, temperature, max_tokens);

        if (is_blank(reply))
            throw std::runtime_error("Model returned an empty response");

        _history.push_back({Role::Assistant, reply});

        // Auto-generate a short title from the first user message (best-effort)
        if (is_first_user_message && !_title.has_value()) {
            try { _title = _generate_title(message); } catch (...) {}
        }

        return reply;

    } catch (...) {
        // Rollback: remove the user message so history stays consistent,
        // then wipe the KV cache because it may be partially out of sync.
        _history.pop_back();
        _reset_kv_cache();
        throw;
    }
}

void LlamaChat::clear_history() noexcept {
    // Erase everything after the system prompt (index 0)
    _history.erase(_history.begin() + 1, _history.end());
    // Wipe KV cache — cached vectors now reference turns that no longer exist
    _reset_kv_cache();
    // Title is intentionally kept — clearing history doesn't rename the chat
}

std::vector<Message> LlamaChat::get_history() const {
    return _history;  // Return a copy so callers cannot mutate internal state
}

std::string LlamaChat::save_history(const std::string& filepath) {
    // ── Serialise to JSON ─────────────────────────────────────────────────────
    json doc;
    doc["title"] = _title.has_value() ? json(*_title) : json(nullptr);

    json messages = json::array();
    for (const auto& msg : _history) {
        messages.push_back({{"role", role_to_string(msg.role)}, {"content", msg.content}});
    }
    doc["messages"] = messages;

    // ── Determine output path ─────────────────────────────────────────────────
    std::string output_path = filepath;
    if (is_blank(output_path)) {
        std::string base;
        if (_title.has_value()) {
            base = *_title;
            for (char& c : base) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
        } else {
            base = "chat_" + timestamp_string();
        }
        output_path = base + ".json";
    }

    std::ofstream ofs(output_path);
    if (!ofs.is_open())
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    ofs << doc.dump(2);  // Pretty-print with 2-space indentation
    if (!ofs)
        throw std::runtime_error("Write failed for: " + output_path);

    return output_path;
}

void LlamaChat::load_history(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open file for reading: " + filepath);

    json doc;
    try {
        ifs >> doc;
    } catch (const json::parse_error& e) {
        throw std::invalid_argument(
            std::string("Malformed JSON in ") + filepath + ": " + e.what());
    }

    if (!doc.is_object())
        throw std::invalid_argument("Expected a JSON object at the top level");
    if (!doc.contains("messages") || !doc["messages"].is_array())
        throw std::invalid_argument("JSON must contain a 'messages' array");

    std::vector<Message> new_history;
    for (const auto& item : doc["messages"]) {
        if (!item.contains("role") || !item["role"].is_string())
            throw std::invalid_argument("Each message must have a string 'role' field");
        if (!item.contains("content") || !item["content"].is_string())
            throw std::invalid_argument("Each message must have a string 'content' field");
        Role role = role_from_string(item["role"].get<std::string>());
        new_history.push_back({role, item["content"].get<std::string>()});
    }

    std::optional<std::string> new_title;
    if (doc.contains("title") && doc["title"].is_string())
        new_title = doc["title"].get<std::string>();

    // Commit only after all validation passes
    _history = std::move(new_history);
    _title   = std::move(new_title);
    // Wipe KV cache — loaded history has never been through llama_decode
    _reset_kv_cache();
}

std::optional<std::string> LlamaChat::get_title() const noexcept {
    return _title;
}

void LlamaChat::set_title(const std::string& title) {
    if (is_blank(title))
        throw std::invalid_argument("title must not be blank");
    _title = title;
}
