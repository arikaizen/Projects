/**
 * convo.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Implementation of convo.hpp.
 *
 * Design notes
 * ────────────
 * AIModel
 *   Owns a llama_model* and a general-purpose inference context.  All stateless
 *   calls (generate, embed, similarity, search) share this single context.
 *   _decode() clears the KV cache at the start of every call so each generate()
 *   is fully independent.  Embedding is handled by a short-lived second context
 *   configured for mean-pool embedding output.
 *
 * AIConvo
 *   Owns a separate conversation context whose KV cache is cleared and fully
 *   reloaded on every chat() call.  _run_chat() always reprocesses the complete
 *   history so the model sees every prior turn without exception.
 *
 *   History rollback: if inference throws after the user message was appended,
 *   the message is popped and _clear_kv_cache() is called so the next chat()
 *   starts from a coherent state.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "convo.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Module-private utilities
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/** Return true if s is empty or contains only whitespace. */
bool is_blank(const std::string& s) {
    for (unsigned char c : s)
        if (!std::isspace(c)) return false;
    return true;
}

/** Euclidean (L2) norm of a float vector. */
float vec_norm(const std::vector<float>& float_vector) {
    float sum_of_squares = std::inner_product(
        float_vector.begin(), float_vector.end(), float_vector.begin(), 0.0f);
    return std::sqrt(sum_of_squares);
}

/** Cosine similarity of two equal-length float vectors.
 *  Returns 0 when either vector has zero magnitude. */
float cosine_sim(const std::vector<float>& vec_a, const std::vector<float>& vec_b) {
    assert(vec_a.size() == vec_b.size());
    float dot_product = std::inner_product(
        vec_a.begin(), vec_a.end(), vec_b.begin(), 0.0f);
    float norm_a = vec_norm(vec_a);
    float norm_b = vec_norm(vec_b);
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot_product / (norm_a * norm_b);
}

/** ISO-8601-like timestamp string (YYYY-MM-DD_HH-MM-SS) for auto filenames. */
std::string now_stamp() {
    auto current_time_point = std::chrono::system_clock::now();
    std::time_t time_as_seconds = std::chrono::system_clock::to_time_t(current_time_point);
    std::tm time_struct{};
#ifdef _WIN32
    localtime_s(&time_struct, &time_as_seconds);
#else
    localtime_r(&time_as_seconds, &time_struct);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d_%H-%M-%S", &time_struct);
    return buf;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Role helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string role_to_str(Role r) {
    switch (r) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
    }
    throw std::invalid_argument("role_to_str: unknown Role value");
}

Role role_from_str(const std::string& s) {
    if (s == "system")    return Role::System;
    if (s == "user")      return Role::User;
    if (s == "assistant") return Role::Assistant;
    throw std::invalid_argument("role_from_str: unknown role \"" + s + "\"");
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIModel::AIModel(const std::string& model_path, int context_size, int thread_count)
    : _model(nullptr)
    , _inference_ctx(nullptr)
    , _context_size(context_size)
    , _thread_count(thread_count)
{
    // Initialize the global llama backend (ref-counted; safe to call multiple times).
    llama_backend_init();

    // ── Load model weights ────────────────────────────────────────────────────
    llama_model_params model_params = llama_model_default_params();
    _model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!_model)
        throw std::runtime_error("AIModel: failed to load model from \"" + model_path + "\"");

    // ── Create inference context ──────────────────────────────────────────────
    llama_context_params context_params    = llama_context_default_params();
    context_params.n_ctx                   = static_cast<uint32_t>(context_size);
    context_params.n_threads               = static_cast<uint32_t>(thread_count);
    context_params.n_threads_batch         = static_cast<uint32_t>(thread_count);

    _inference_ctx = llama_new_context_with_model(_model, context_params);
    if (!_inference_ctx) {
        llama_model_free(_model);
        _model = nullptr;
        throw std::runtime_error(
            "AIModel: failed to create inference context for \"" + model_path + "\"");
    }
}

AIModel::~AIModel() noexcept {
    if (_inference_ctx) { llama_free(_inference_ctx); _inference_ctx = nullptr; }
    if (_model)         { llama_model_free(_model);   _model         = nullptr; }
    llama_backend_free();
}

AIModel::AIModel(AIModel&& other) noexcept
    : _model(other._model)
    , _inference_ctx(other._inference_ctx)
    , _context_size(other._context_size)
    , _thread_count(other._thread_count)
    , _embedding_cache(std::move(other._embedding_cache))
{
    other._model         = nullptr;
    other._inference_ctx = nullptr;
}

AIModel& AIModel::operator=(AIModel&& other) noexcept {
    if (this != &other) {
        if (_inference_ctx) llama_free(_inference_ctx);
        if (_model)         llama_model_free(_model);

        _model           = other._model;
        _inference_ctx   = other._inference_ctx;
        _context_size    = other._context_size;
        _thread_count    = other._thread_count;
        _embedding_cache = std::move(other._embedding_cache);

        other._model         = nullptr;
        other._inference_ctx = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> AIModel::_tokenize(const std::string& text, bool add_bos) const {
    // First call with a null buffer returns the (negative) required token count.
    int token_count = llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,
        add_bos,
        /*special=*/false
    );
    if (token_count < 0) token_count = -token_count;

    std::vector<llama_token> token_list(static_cast<std::size_t>(token_count));
    llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        token_list.data(), static_cast<int32_t>(token_list.size()),
        add_bos,
        /*special=*/false
    );
    return token_list;
}

std::string AIModel::_decode(const std::vector<llama_token>& tokens,
                              float temperature, int max_tokens) const {
    // Each generate() is stateless — clear any KV vectors left by a previous call.
    llama_memory_clear(llama_get_memory(_inference_ctx), /*data=*/true);

    // Feed the full prompt into the context in one batch.
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );
    if (llama_decode(_inference_ctx, prompt_batch) != 0)
        throw std::runtime_error("AIModel::_decode: llama_decode failed during prompt evaluation");

    // Build a sampler chain: temperature → top-p (nucleus) → distribution draw.
    llama_sampler_chain_params sampler_chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_chain_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string generated_text;
    generated_text.reserve(static_cast<std::size_t>(max_tokens) * 4);

    int32_t tokens_processed_count = static_cast<int32_t>(tokens.size());

    for (int i = 0; i < max_tokens; ++i) {
        llama_token current_token = llama_sampler_sample(sampler, _inference_ctx, -1);

        if (llama_token_is_eog(_model, current_token)) break;

        // Decode the token id to its UTF-8 string piece (up to 32 bytes).
        char piece[32];
        int  piece_length = llama_token_to_piece(
            _model, current_token, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            generated_text.append(piece, static_cast<std::size_t>(piece_length));

        // Feed the newly generated token back for the next step.
        llama_batch next_batch = llama_batch_get_one(&current_token, 1);
        if (llama_decode(_inference_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("AIModel::_decode: llama_decode failed during generation");
        }
        ++tokens_processed_count;
    }

    llama_sampler_free(sampler);
    return generated_text;
}

std::vector<float> AIModel::_raw_embed(const std::string& text) const {
    // Create a short-lived context configured for mean-pooled embeddings.
    llama_context_params embedding_params  = llama_context_default_params();
    embedding_params.n_ctx                 = 512;
    embedding_params.n_threads             = static_cast<uint32_t>(_thread_count);
    embedding_params.embeddings            = true;
    embedding_params.pooling_type          = LLAMA_POOLING_TYPE_MEAN;

    llama_context* embedding_ctx = llama_new_context_with_model(_model, embedding_params);
    if (!embedding_ctx)
        throw std::runtime_error("AIModel::_raw_embed: failed to create embedding context");

    // Embed without BOS — it disrupts mean pooling.
    std::vector<llama_token> token_list = _tokenize(text, /*add_bos=*/false);

    llama_batch batch = llama_batch_get_one(
        token_list.data(), static_cast<int32_t>(token_list.size()));
    if (llama_decode(embedding_ctx, batch) != 0) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModel::_raw_embed: llama_decode failed");
    }

    int          embedding_dimension  = llama_n_embd(_model);
    const float* embedding_data_ptr   = llama_get_embeddings_seq(embedding_ctx, 0);
    if (!embedding_data_ptr) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModel::_raw_embed: no embedding output "
                                 "(does this model support embeddings?)");
    }

    std::vector<float> result(embedding_data_ptr, embedding_data_ptr + embedding_dimension);
    llama_free(embedding_ctx);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModel::generate(const std::string& prompt,
                               float temperature,
                               int   max_tokens) const {
    if (is_blank(prompt))
        throw std::invalid_argument("AIModel::generate: prompt must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "AIModel::generate: temperature must be in [0.0, 2.0]; got "
            + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "AIModel::generate: max_tokens must be >= 1; got "
            + std::to_string(max_tokens));

    auto token_list    = _tokenize(prompt, /*add_bos=*/true);
    auto generated_text = _decode(token_list, temperature, max_tokens);

    if (is_blank(generated_text))
        throw std::runtime_error("AIModel::generate: model returned an empty response");

    return generated_text;
}

std::vector<float> AIModel::embed(const std::string& text, bool use_cache) {
    if (is_blank(text))
        throw std::invalid_argument("AIModel::embed: text must not be blank");

    // Return cached vector when available.
    if (use_cache) {
        auto cache_entry = _embedding_cache.find(text);
        if (cache_entry != _embedding_cache.end()) return cache_entry->second;
    }

    auto result = _raw_embed(text);

    if (vec_norm(result) == 0.0f)
        throw std::runtime_error(
            "AIModel::embed: embedding has zero magnitude "
            "(model may not support embeddings)");

    if (use_cache)
        _embedding_cache[text] = result;

    return result;
}

float AIModel::similarity(const std::string& a, const std::string& b) {
    if (is_blank(a)) throw std::invalid_argument("AIModel::similarity: first text must not be blank");
    if (is_blank(b)) throw std::invalid_argument("AIModel::similarity: second text must not be blank");
    return cosine_sim(embed(a), embed(b));
}

std::vector<std::pair<float, std::string>>
AIModel::search(const std::string&              query,
                const std::vector<std::string>& labels,
                const std::vector<std::string>& texts,
                int top_n) {
    if (is_blank(query))
        throw std::invalid_argument("AIModel::search: query must not be blank");
    if (labels.size() != texts.size())
        throw std::invalid_argument(
            "AIModel::search: labels and texts must have the same length; got "
            + std::to_string(labels.size()) + " vs " + std::to_string(texts.size()));
    if (top_n < 1)
        throw std::invalid_argument(
            "AIModel::search: top_n must be >= 1; got " + std::to_string(top_n));

    auto query_embedding = embed(query);

    std::vector<std::pair<float, std::string>> ranked_results;
    ranked_results.reserve(texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i) {
        float score = cosine_sim(query_embedding, embed(texts[i]));
        ranked_results.emplace_back(score, labels[i]);
    }

    std::sort(ranked_results.begin(), ranked_results.end(),
              [](const auto& x, const auto& y) { return x.first > y.first; });

    int results_to_return = std::min(top_n, static_cast<int>(ranked_results.size()));
    return {ranked_results.begin(), ranked_results.begin() + results_to_return};
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIConvo::AIConvo(AIModel& model, const std::string& system_prompt)
    : _model(model)
    , _system_prompt(system_prompt)
    , _conversation_ctx(nullptr)
    , _tokens_processed(0)
{
    if (is_blank(system_prompt))
        throw std::invalid_argument("AIConvo: system_prompt must not be blank");

    // Allocate a dedicated context whose KV cache belongs to this conversation.
    llama_context_params context_params  = llama_context_default_params();
    context_params.n_ctx                 = static_cast<uint32_t>(model._context_size);
    context_params.n_threads             = static_cast<uint32_t>(model._thread_count);
    context_params.n_threads_batch       = static_cast<uint32_t>(model._thread_count);

    _conversation_ctx = llama_new_context_with_model(model.model_ptr(), context_params);
    if (!_conversation_ctx)
        throw std::runtime_error("AIConvo: failed to create dedicated chat context");

    // Seed the history with the system message.
    _history.push_back({Role::System, system_prompt});
}

AIConvo::~AIConvo() noexcept {
    if (_conversation_ctx) { llama_free(_conversation_ctx); _conversation_ctx = nullptr; }
}

AIConvo::AIConvo(AIConvo&& other) noexcept
    : _model(other._model)
    , _system_prompt(std::move(other._system_prompt))
    , _history(std::move(other._history))
    , _title(std::move(other._title))
    , _conversation_ctx(other._conversation_ctx)
    , _tokens_processed(other._tokens_processed)
{
    other._conversation_ctx  = nullptr;
    other._tokens_processed  = 0;
}

AIConvo& AIConvo::operator=(AIConvo&& other) noexcept {
    if (this != &other) {
        if (_conversation_ctx) llama_free(_conversation_ctx);

        // _model is a reference — both sides must already refer to the same object.
        _system_prompt      = std::move(other._system_prompt);
        _history            = std::move(other._history);
        _title              = std::move(other._title);
        _conversation_ctx   = other._conversation_ctx;
        _tokens_processed   = other._tokens_processed;

        other._conversation_ctx  = nullptr;
        other._tokens_processed  = 0;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — private helpers
// ─────────────────────────────────────────────────────────────────────────────

void AIConvo::_clear_kv_cache() noexcept {
    // Wipe every cached key/value vector and reset the position counter.
    // Called at the start of every _run_chat() so each turn begins with a
    // clean slate and processes the complete conversation history.
    llama_memory_clear(llama_get_memory(_conversation_ctx), /*data=*/true);
    _tokens_processed = 0;
}

std::string AIConvo::_build_prompt() const {
    // Build a C array of llama_chat_message structs from our history.
    // We keep role strings alive in a parallel vector so their c_str() pointers
    // remain valid while the chat_message_array is being used.
    std::vector<std::string>        role_strings;
    std::vector<llama_chat_message> chat_message_array;
    role_strings.reserve(_history.size());
    chat_message_array.reserve(_history.size());

    for (const auto& message : _history) {
        role_strings.push_back(role_to_str(message.role));
        chat_message_array.push_back(
            {role_strings.back().c_str(), message.content.c_str()});
    }

    // Dry-run: pass null buffer to get the required byte count.
    int required_buffer_size = llama_chat_apply_template(
        _model.model_ptr(),
        nullptr,         // use the model's built-in template
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        nullptr, 0
    );

    if (required_buffer_size < 0) {
        // Model has no built-in template; fall back to a plain-text format.
        std::ostringstream output_stream;
        for (const auto& message : _history)
            output_stream << role_to_str(message.role) << ": " << message.content << "\n";
        output_stream << "assistant: ";
        return output_stream.str();
    }

    std::vector<char> formatted_prompt_buffer(
        static_cast<std::size_t>(required_buffer_size) + 1, '\0');
    llama_chat_apply_template(
        _model.model_ptr(),
        nullptr,
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        formatted_prompt_buffer.data(), static_cast<int>(formatted_prompt_buffer.size())
    );

    return {formatted_prompt_buffer.data(), static_cast<std::size_t>(required_buffer_size)};
}

std::string AIConvo::_run_chat(const std::vector<llama_token>& all_tokens,
                                float temperature, int max_tokens) {
    // ── Statefulness guarantee ────────────────────────────────────────────────
    // all_tokens is the full formatted prompt built from _history, which always
    // contains every system/user/assistant message in chronological order.
    // We reprocess the entire prompt on every turn so the model is guaranteed to
    // see the complete conversation context regardless of any prior state.
    _clear_kv_cache();

    // ── Context overflow check ────────────────────────────────────────────────
    if (static_cast<int>(all_tokens.size()) >= _model.ctx_size()) {
        throw std::runtime_error(
            "AIConvo::_run_chat: conversation is too long for the context window ("
            + std::to_string(all_tokens.size()) + " prompt tokens, context limit "
            + std::to_string(_model.ctx_size()) + ")");
    }

    // ── Feed the full prompt into a clean KV cache ────────────────────────────
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(all_tokens.data()),
        static_cast<int32_t>(all_tokens.size())
    );
    if (llama_decode(_conversation_ctx, prompt_batch) != 0)
        throw std::runtime_error(
            "AIConvo::_run_chat: llama_decode failed during prompt evaluation");

    _tokens_processed = static_cast<int32_t>(all_tokens.size());

    // ── Sampler chain: temperature → top-p → draw ────────────────────────────
    llama_sampler_chain_params sampler_chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_chain_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string reply;
    reply.reserve(static_cast<std::size_t>(max_tokens) * 4);

    for (int i = 0; i < max_tokens; ++i) {
        llama_token current_token = llama_sampler_sample(sampler, _conversation_ctx, -1);

        if (llama_token_is_eog(_model.model_ptr(), current_token)) break;

        char piece[32];
        int  piece_length = llama_token_to_piece(
            _model.model_ptr(), current_token, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            reply.append(piece, static_cast<std::size_t>(piece_length));

        llama_batch next_batch = llama_batch_get_one(&current_token, 1);
        if (llama_decode(_conversation_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error(
                "AIConvo::_run_chat: llama_decode failed during generation");
        }
        ++_tokens_processed;
    }

    llama_sampler_free(sampler);
    return reply;
}

std::string AIConvo::_make_title(const std::string& first_msg) {
    const std::string title_prompt =
        "In 5 words or fewer, give a short title for this conversation.\n"
        "Message: " + first_msg + "\nTitle:";
    try {
        std::string raw_title = _model.generate(
            title_prompt, /*temperature=*/0.3f, /*max_tokens=*/20);
        auto title_start = raw_title.find_first_not_of(" \t\n\r");
        auto title_end   = raw_title.find_last_not_of(" \t\n\r");
        if (title_start == std::string::npos)
            return first_msg.substr(0, 40);
        return raw_title.substr(title_start, title_end - title_start + 1);
    } catch (...) {
        // Title generation is optional; fall back to a truncated first message.
        return first_msg.substr(0, std::min<std::size_t>(40, first_msg.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string AIConvo::chat(const std::string& message,
                           float temperature,
                           int   max_tokens) {
    if (is_blank(message))
        throw std::invalid_argument("AIConvo::chat: message must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "AIConvo::chat: temperature must be in [0.0, 2.0]; got "
            + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "AIConvo::chat: max_tokens must be >= 1; got "
            + std::to_string(max_tokens));

    // Remember whether this is the very first user turn (for title generation).
    bool is_first_user_message = true;
    for (const auto& history_entry : _history)
        if (history_entry.role == Role::User) { is_first_user_message = false; break; }

    // Append the user message optimistically — roll back if inference fails.
    _history.push_back({Role::User, message});

    try {
        // _build_prompt() serialises ALL of _history (system + every user/assistant
        // pair so far + the new user message + the assistant-turn prefix).
        // _run_chat() always reprocesses this full prompt from scratch, so the model
        // sees the complete conversation history on every single turn.
        std::string full_prompt    = _build_prompt();
        auto        token_list     = _model._tokenize(full_prompt, /*add_bos=*/true);
        std::string reply          = _run_chat(token_list, temperature, max_tokens);

        if (is_blank(reply))
            throw std::runtime_error("AIConvo::chat: model returned an empty response");

        _history.push_back({Role::Assistant, reply});

        // Auto-generate a title from the first user message (best-effort).
        if (is_first_user_message && !_title.has_value()) {
            try { _title = _make_title(message); }
            catch (...) {}  // title is optional
        }

        return reply;

    } catch (...) {
        // Roll back the user message and clear the KV cache.
        _history.pop_back();
        _clear_kv_cache();
        throw;
    }
}

void AIConvo::clear_history() noexcept {
    // Keep only the system message (always index 0).
    if (_history.size() > 1)
        _history.erase(_history.begin() + 1, _history.end());
    _clear_kv_cache();
    // Title is intentionally preserved — clearing turns does not rename the chat.
}

std::vector<Message> AIConvo::get_history() const {
    return _history;  // return a copy so callers cannot mutate internal state
}

std::string AIConvo::save_history(const std::string& path) {
    json json_document;
    json_document["title"] = _title.has_value() ? json(*_title) : json(nullptr);

    json json_messages = json::array();
    for (const auto& history_entry : _history)
        json_messages.push_back({
            {"role",    role_to_str(history_entry.role)},
            {"content", history_entry.content}
        });
    json_document["messages"] = json_messages;

    // Determine output path: use provided path, or auto-generate one.
    std::string output_file_path = path;
    if (is_blank(output_file_path)) {
        std::string base_filename;
        if (_title.has_value()) {
            base_filename = *_title;
            for (char& c : base_filename)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
        } else {
            base_filename = "chat_" + now_stamp();
        }
        output_file_path = base_filename + ".json";
    }

    std::ofstream output_file(output_file_path);
    if (!output_file.is_open())
        throw std::runtime_error(
            "AIConvo::save_history: cannot open \"" + output_file_path + "\" for writing");

    output_file << json_document.dump(2);
    if (!output_file)
        throw std::runtime_error(
            "AIConvo::save_history: write failed for \"" + output_file_path + "\"");

    return output_file_path;
}

void AIConvo::load_history(const std::string& path) {
    std::ifstream input_file(path);
    if (!input_file.is_open())
        throw std::runtime_error("AIConvo::load_history: cannot open \"" + path + "\"");

    json json_document;
    try {
        input_file >> json_document;
    } catch (const json::parse_error& parse_error) {
        throw std::invalid_argument(
            std::string("AIConvo::load_history: malformed JSON in \"")
            + path + "\": " + parse_error.what());
    }

    if (!json_document.is_object())
        throw std::invalid_argument(
            "AIConvo::load_history: expected a JSON object at top level");
    if (!json_document.contains("messages") || !json_document["messages"].is_array())
        throw std::invalid_argument(
            "AIConvo::load_history: JSON must contain a \"messages\" array");

    std::vector<Message> loaded_history;
    for (const auto& item : json_document["messages"]) {
        if (!item.contains("role") || !item["role"].is_string())
            throw std::invalid_argument(
                "AIConvo::load_history: each message must have a string \"role\"");
        if (!item.contains("content") || !item["content"].is_string())
            throw std::invalid_argument(
                "AIConvo::load_history: each message must have a string \"content\"");

        Role role = role_from_str(item["role"].get<std::string>());
        loaded_history.push_back({role, item["content"].get<std::string>()});
    }

    std::optional<std::string> loaded_title;
    if (json_document.contains("title") && json_document["title"].is_string())
        loaded_title = json_document["title"].get<std::string>();

    // Commit only after all validation succeeds.
    _history = std::move(loaded_history);
    _title   = std::move(loaded_title);
    _clear_kv_cache();  // loaded history has no cached KV vectors yet
}

std::optional<std::string> AIConvo::get_title() const noexcept {
    return _title;
}

void AIConvo::set_title(const std::string& title) {
    if (is_blank(title))
        throw std::invalid_argument("AIConvo::set_title: title must not be blank");
    _title = title;
}
