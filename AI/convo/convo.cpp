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
 *   reloaded on every Chat() call.  RunChat() always reprocesses the complete
 *   history so the model sees every prior turn without exception.
 *
 *   History rollback: if inference throws after the user message was appended,
 *   the message is popped and _clear_kv_cache() is called so the next chat()
 *   starts from a coherent state.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "convo.hpp"

#include <algorithm>  // std::min, std::sort
#include <cassert>    // assert
#include <chrono>     // std::chrono::system_clock
#include <cmath>      // std::sqrt
#include <cstring>    // std::strlen
#include <fstream>    // std::ifstream, std::ofstream
#include <numeric>    // std::inner_product
#include <sstream>    // std::ostringstream
#include <stdexcept>  // std::invalid_argument, std::runtime_error

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Module-private utilities
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/** Return true if s is empty or contains only whitespace. */
bool IsBlank(const std::string& s) {
    for (unsigned char c : s)
        if (!std::isspace(c)) return false;
    return true;
}

/** Euclidean (L2) norm of a float vector. */
float VecNorm(const std::vector<float>& float_vector) {
    float sum_of_squares = std::inner_product(
        float_vector.begin(), float_vector.end(), float_vector.begin(), 0.0f);
    return std::sqrt(sum_of_squares);
}

/** Cosine similarity of two equal-length float vectors.
 *  Returns 0 when either vector has zero magnitude. */
float CosineSim(const std::vector<float>& vec_a, const std::vector<float>& vec_b) {
    assert(vec_a.size() == vec_b.size());
    float dot_product = std::inner_product(
        vec_a.begin(), vec_a.end(), vec_b.begin(), 0.0f);
    float norm_a = VecNorm(vec_a);
    float norm_b = VecNorm(vec_b);
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot_product / (norm_a * norm_b);
}

/** ISO-8601-like timestamp string (YYYY-MM-DD_HH-MM-SS) for auto filenames. */
std::string NowStamp() {
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

std::string RoleToStr(Role r) {
    switch (r) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
    }
    throw std::invalid_argument("RoleToStr: unknown Role value");
}

Role RoleFromStr(const std::string& s) {
    if (s == "system")    return Role::System;
    if (s == "user")      return Role::User;
    if (s == "assistant") return Role::Assistant;
    throw std::invalid_argument("RoleFromStr: unknown role \"" + s + "\"");
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIModel::AIModel(const std::string& model_path, int context_size, int thread_count)
    : m_model(nullptr)
    , m_inference_ctx(nullptr)
    , m_context_size(context_size)
    , m_thread_count(thread_count)
{
    // Initialize the global llama backend (ref-counted; safe to call multiple times).
    llama_backend_init();

    // ── Load model weights ────────────────────────────────────────────────────
    llama_model_params model_params = llama_model_default_params();
    m_model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!m_model)
        throw std::runtime_error("AIModel: failed to load model from \"" + model_path + "\"");

    // ── Create inference context ──────────────────────────────────────────────
    llama_context_params context_params    = llama_context_default_params();
    context_params.n_ctx                   = static_cast<uint32_t>(context_size);
    context_params.n_batch                 = static_cast<uint32_t>(context_size);
    context_params.n_threads               = static_cast<uint32_t>(thread_count);
    context_params.n_threads_batch         = static_cast<uint32_t>(thread_count);

    m_inference_ctx = llama_init_from_model(m_model, context_params);
    if (!m_inference_ctx) {
        llama_model_free(m_model);
        m_model = nullptr;
        throw std::runtime_error(
            "AIModel: failed to create inference context for \"" + model_path + "\"");
    }
}

AIModel::~AIModel() noexcept {
    if (m_inference_ctx) { llama_free(m_inference_ctx); m_inference_ctx = nullptr; }
    if (m_model)         { llama_model_free(m_model);   m_model         = nullptr; }
    llama_backend_free();
}

AIModel::AIModel(AIModel&& other) noexcept
    : m_model(other.m_model)
    , m_inference_ctx(other.m_inference_ctx)
    , m_context_size(other.m_context_size)
    , m_thread_count(other.m_thread_count)
    , m_embedding_cache(std::move(other.m_embedding_cache))
{
    other.m_model         = nullptr;
    other.m_inference_ctx = nullptr;
}

AIModel& AIModel::operator=(AIModel&& other) noexcept {
    if (this != &other) {
        if (m_inference_ctx) llama_free(m_inference_ctx);
        if (m_model)         llama_model_free(m_model);

        m_model           = other.m_model;
        m_inference_ctx   = other.m_inference_ctx;
        m_context_size    = other.m_context_size;
        m_thread_count    = other.m_thread_count;
        m_embedding_cache = std::move(other.m_embedding_cache);

        other.m_model         = nullptr;
        other.m_inference_ctx = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> AIModel::Tokenize(const std::string& text, bool add_bos) const {
    // First call with a null buffer returns the (negative) required token count.
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    int token_count = llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,
        add_bos,
        /*special=*/false
    );
    if (token_count < 0) token_count = -token_count;

    std::vector<llama_token> token_list(static_cast<std::size_t>(token_count));
    llama_tokenize(
        vocab,
        text.c_str(), static_cast<int32_t>(text.size()),
        token_list.data(), static_cast<int32_t>(token_list.size()),
        add_bos,
        /*special=*/false
    );
    return token_list;
}

std::string AIModel::Decode(const std::vector<llama_token>& tokens,
                            float temperature,
                            int   max_tokens) const {
    // Each generate() is stateless — clear any KV vectors left by a previous call.
    llama_memory_clear(llama_get_memory(m_inference_ctx), /*data=*/true);

    // Feed the full prompt into the context in one batch.
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );
    if (llama_decode(m_inference_ctx, prompt_batch) != 0)
        throw std::runtime_error("AIModel::Decode: llama_decode failed during prompt evaluation");

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
        llama_token current_token = llama_sampler_sample(sampler, m_inference_ctx, -1);

        const llama_vocab* vocab = llama_model_get_vocab(m_model);
        if (llama_vocab_is_eog(vocab, current_token)) break;

        // Decode the token id to its UTF-8 string piece (up to 32 bytes).
        char piece[32];
        int  piece_length = llama_token_to_piece(
            vocab, current_token, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            generated_text.append(piece, static_cast<std::size_t>(piece_length));

        // Feed the newly generated token back for the next step.
        llama_batch next_batch = llama_batch_get_one(&current_token, 1);
        if (llama_decode(m_inference_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("AIModel::Decode: llama_decode failed during generation");
        }
        ++tokens_processed_count;
    }

    llama_sampler_free(sampler);
    return generated_text;
}

std::vector<float> AIModel::RawEmbed(const std::string& text) const {
    // Create a short-lived context configured for mean-pooled embeddings.
    llama_context_params embedding_params  = llama_context_default_params();
    embedding_params.n_ctx                 = 512;
    embedding_params.n_threads             = static_cast<uint32_t>(m_thread_count);
    embedding_params.embeddings            = true;
    embedding_params.pooling_type          = LLAMA_POOLING_TYPE_MEAN;

    llama_context* embedding_ctx = llama_init_from_model(m_model, embedding_params);
    if (!embedding_ctx)
        throw std::runtime_error("AIModel::RawEmbed: failed to create embedding context");

    // Embed without BOS — it disrupts mean pooling.
    std::vector<llama_token> token_list = Tokenize(text, /*add_bos=*/false);

    llama_batch batch = llama_batch_get_one(
        token_list.data(), static_cast<int32_t>(token_list.size()));
    if (llama_decode(embedding_ctx, batch) != 0) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModel::RawEmbed: llama_decode failed");
    }

    int          embedding_dimension  = llama_model_n_embd(m_model);
    const float* embedding_data_ptr   = llama_get_embeddings_seq(embedding_ctx, 0);
    if (!embedding_data_ptr) {
        llama_free(embedding_ctx);
        throw std::runtime_error("AIModel::RawEmbed: no embedding output "
                                 "(does this model support embeddings?)");
    }

    std::vector<float> result(embedding_data_ptr, embedding_data_ptr + embedding_dimension);
    llama_free(embedding_ctx);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — public API
// ─────────────────────────────────────────────────────────────────────────────

std::string AIModel::Generate(const std::string& prompt,
                              float temperature,
                              int   max_tokens) const {
    if (IsBlank(prompt))
        throw std::invalid_argument("AIModel::Generate: prompt must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "AIModel::Generate: temperature must be in [0.0, 2.0]; got "
            + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "AIModel::Generate: max_tokens must be >= 1; got "
            + std::to_string(max_tokens));

    auto token_list     = Tokenize(prompt, /*add_bos=*/true);
    auto generated_text = Decode(token_list, temperature, max_tokens);

    if (IsBlank(generated_text))
        throw std::runtime_error("AIModel::Generate: model returned an empty response");

    return generated_text;
}

std::vector<float> AIModel::Embed(const std::string& text, bool use_cache) {
    if (IsBlank(text))
        throw std::invalid_argument("AIModel::Embed: text must not be blank");

    // Return cached vector when available.
    if (use_cache) {
        auto cache_entry = m_embedding_cache.find(text);
        if (cache_entry != m_embedding_cache.end()) return cache_entry->second;
    }

    auto result = RawEmbed(text);

    if (VecNorm(result) == 0.0f){}
        throw std::runtime_error(
            "AIModel::Embed: embedding has zero magnitude "
            "(model may not support embeddings)");

    if (use_cache)
        m_embedding_cache[text] = result;

    return result;
}

float AIModel::Similarity(const std::string& a, const std::string& b) {
    if (IsBlank(a)) throw std::invalid_argument("AIModel::Similarity: first text must not be blank");
    if (IsBlank(b)) throw std::invalid_argument("AIModel::Similarity: second text must not be blank");
    return CosineSim(Embed(a), Embed(b));
}

std::vector<std::pair<float, std::string>>
AIModel::Search(const std::string&               query,
                const std::vector<std::string>&  labels,
                const std::vector<std::string>&  texts,
                int                              top_n) {
    if (IsBlank(query))
        throw std::invalid_argument("AIModel::Search: query must not be blank");
    if (labels.size() != texts.size())
        throw std::invalid_argument(
            "AIModel::Search: labels and texts must have the same length; got "
            + std::to_string(labels.size()) + " vs " + std::to_string(texts.size()));
    if (top_n < 1)
        throw std::invalid_argument(
            "AIModel::Search: top_n must be >= 1; got " + std::to_string(top_n));

    auto query_embedding = Embed(query);

    std::vector<std::pair<float, std::string>> ranked_results;
    ranked_results.reserve(texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i) {
        float score = CosineSim(query_embedding, Embed(texts[i]));
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
    : m_model(model)
    , m_system_prompt(system_prompt)
    , m_conversation_ctx(nullptr)
    , m_recent_turns_window(10)
    , m_tokens_processed(0)
    , m_clear_kv_each_turn(true)
{
    if (IsBlank(system_prompt))
        throw std::invalid_argument("AIConvo: system_prompt must not be blank");

    // Allocate a dedicated context whose KV cache belongs to this conversation.
    llama_context_params context_params  = llama_context_default_params();
    context_params.n_ctx                 = static_cast<uint32_t>(model.m_context_size);
    context_params.n_batch               = static_cast<uint32_t>(model.m_context_size);
    context_params.n_threads             = static_cast<uint32_t>(model.m_thread_count);
    context_params.n_threads_batch       = static_cast<uint32_t>(model.m_thread_count);

    m_conversation_ctx = llama_init_from_model(model.ModelPtr(), context_params);
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvo: failed to create dedicated chat context");

    // Seed the history with the system message.
    m_history.push_back({Role::System, system_prompt});
}

AIConvo::~AIConvo() noexcept {
    if (m_conversation_ctx) { llama_free(m_conversation_ctx); m_conversation_ctx = nullptr; }
}

AIConvo::AIConvo(AIConvo&& other) noexcept
    : m_model(other.m_model)
    , m_system_prompt(std::move(other.m_system_prompt))
    , m_history(std::move(other.m_history))
    , m_title(std::move(other.m_title))
    , m_conversation_ctx(other.m_conversation_ctx)
    , m_tokens_processed(other.m_tokens_processed)
    , m_clear_kv_each_turn(other.m_clear_kv_each_turn)
    , m_cached_prompt_tokens(std::move(other.m_cached_prompt_tokens))
{
    other.m_conversation_ctx  = nullptr;
    other.m_tokens_processed  = 0;
    other.m_cached_prompt_tokens.clear();
}

AIConvo& AIConvo::operator=(AIConvo&& other) noexcept {
    if (this != &other) {
        if (m_conversation_ctx) llama_free(m_conversation_ctx);

        // m_model is a reference — both sides must already refer to the same object.
        m_system_prompt      = std::move(other.m_system_prompt);
        m_history            = std::move(other.m_history);
        m_title              = std::move(other.m_title);
        m_conversation_ctx   = other.m_conversation_ctx;
        m_tokens_processed   = other.m_tokens_processed;
        m_clear_kv_each_turn = other.m_clear_kv_each_turn;
        m_cached_prompt_tokens = std::move(other.m_cached_prompt_tokens);

        other.m_conversation_ctx  = nullptr;
        other.m_tokens_processed  = 0;
        other.m_cached_prompt_tokens.clear();
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — private helpers
// ─────────────────────────────────────────────────────────────────────────────

void AIConvo::ClearKvCache() noexcept {
    // Wipe every cached key/value vector and reset the position counter.
    // Called at the start of every RunChat() so each turn begins with a
    // clean slate and processes the complete conversation history.
    llama_memory_clear(llama_get_memory(m_conversation_ctx), /*data=*/true);
    m_tokens_processed = 0;
    m_cached_prompt_tokens.clear();
}

void AIConvo::SetClearKvCacheEachTurn(bool clear) noexcept {
    if (m_clear_kv_each_turn == clear) return;
    m_clear_kv_each_turn = clear;
    // Switching modes invalidates any previously cached token prefix.
    ClearKvCache();
}

bool AIConvo::GetClearKvCacheEachTurn() const noexcept {
    return m_clear_kv_each_turn;
}

void AIConvo::SetSystemPromptFile(const std::string& path) {
    if (IsBlank(path))
        throw std::invalid_argument("AIConvo::SetSystemPromptFile: path must not be blank");
    m_system_prompt_file = path;
    // Keep history intact; prompt assembly will load from file on demand.
    ClearKvCache();
}

std::optional<std::string> AIConvo::GetSystemPromptFile() const noexcept {
    return m_system_prompt_file;
}

void AIConvo::SetRecentTurnsWindow(int turns) {
    if (turns < 0)
        throw std::invalid_argument("AIConvo::SetRecentTurnsWindow: turns must be >= 0");
    m_recent_turns_window = turns;
    ClearKvCache();
}

int AIConvo::GetRecentTurnsWindow() const noexcept {
    return m_recent_turns_window;
}

std::string AIConvo::GetSummary() const {
    return m_summary;
}

std::string AIConvo::LoadSystemPromptText() const {
    if (m_system_prompt_file.has_value()) {
        std::ifstream in(*m_system_prompt_file);
        if (!in.is_open())
            throw std::runtime_error("AIConvo: cannot open system prompt file \"" + *m_system_prompt_file + "\"");
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string s = ss.str();
        if (IsBlank(s))
            throw std::runtime_error("AIConvo: system prompt file is blank \"" + *m_system_prompt_file + "\"");
        return s;
    }
    return m_system_prompt;
}

std::vector<Message> AIConvo::GetRecentWindowMessages() const {
    // We keep full history in _history for persistence, but only include the last
    // N user/assistant *pairs* in the prompt for context efficiency.
    if (m_recent_turns_window <= 0) return {};

    std::vector<Message> non_system;
    non_system.reserve(m_history.size());
    for (std::size_t i = 0; i < m_history.size(); ++i) {
        if (m_history[i].role == Role::System) continue;
        non_system.push_back(m_history[i]);
    }

    // Each "chat" is usually two messages (User + Assistant). We keep the tail.
    const std::size_t max_msgs = static_cast<std::size_t>(m_recent_turns_window) * 2;
    if (non_system.size() <= max_msgs) return non_system;
    return {non_system.end() - static_cast<std::ptrdiff_t>(max_msgs), non_system.end()};
}

std::string AIConvo::BuildPrompt() const {
    // Build a C array of llama_chat_message structs from:
    // - system prompt (loaded from file if configured)
    // - hidden summary (if present)
    // - recent conversation window (last N user/assistant pairs)
    // We keep role strings alive in a parallel vector so their c_str() pointers
    // remain valid while the chat_message_array is being used.
    std::vector<std::string>        role_strings;
    std::vector<llama_chat_message> chat_message_array;
    std::vector<Message> prompt_messages;
    prompt_messages.reserve(2 + m_history.size());
    prompt_messages.push_back({Role::System, LoadSystemPromptText()});
    if (!IsBlank(m_summary)) {
        prompt_messages.push_back({Role::System, std::string("Conversation summary (hidden state):\n") + m_summary});
    }
    const auto window = GetRecentWindowMessages();
    prompt_messages.insert(prompt_messages.end(), window.begin(), window.end());

    role_strings.reserve(prompt_messages.size());
    chat_message_array.reserve(prompt_messages.size());

    for (const auto& message : prompt_messages) {
        role_strings.push_back(RoleToStr(message.role));
        chat_message_array.push_back({role_strings.back().c_str(), message.content.c_str()});
    }

    // Get the model's built-in chat template string (nullptr = use model default).
    const char* chat_template = llama_model_chat_template(m_model.ModelPtr(), nullptr);

    // Dry-run: pass null buffer to get the required byte count.
    int required_buffer_size = llama_chat_apply_template(
        chat_template,
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        nullptr, 0
    );

    if (required_buffer_size < 0) {
        // Model has no built-in template; fall back to a plain-text format.
        std::ostringstream output_stream;
        for (const auto& message : m_history)
            output_stream << RoleToStr(message.role) << ": " << message.content << "\n";
        output_stream << "assistant: ";
        return output_stream.str();
    }

    std::vector<char> formatted_prompt_buffer(
        static_cast<std::size_t>(required_buffer_size) + 1, '\0');
    llama_chat_apply_template(
        chat_template,
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        formatted_prompt_buffer.data(), static_cast<int>(formatted_prompt_buffer.size())
    );

    return {formatted_prompt_buffer.data(), static_cast<std::size_t>(required_buffer_size)};
}

void AIConvo::UpdateSummaryNoThrow() {
    // Runs a hidden, stateless update to keep _summary fresh.
    // This uses the shared AIModel (separate from the conversation context).
    try {
        if (m_history.size() < 2) return;  // system only

        // Use recent window to keep the summarizer bounded.
        const auto window = GetRecentWindowMessages();
        std::ostringstream transcript;
        for (const auto& m : window) {
            transcript << RoleToStr(m.role) << ": " << m.content << "\n";
        }

        const std::string prompt =
            "You are a background process that maintains a compact summary of a conversation.\n"
            "The user never sees your output.\n"
            "Update the summary based on the recent transcript.\n"
            "Rules:\n"
            "- Keep it under 200 words.\n"
            "- Include only stable facts, decisions, preferences, and important context.\n"
            "- Do not include speculative content.\n\n"
            "Previous summary:\n"
            + (IsBlank(m_summary) ? std::string("(none)\n") : (m_summary + "\n")) +
            "\nRecent transcript:\n" + transcript.str() +
            "\nNew summary:";

        const std::string new_summary = m_model.Generate(prompt, /*temperature=*/0.2f, /*max_tokens=*/256);
        if (!IsBlank(new_summary)) m_summary = new_summary;
    } catch (...) {
        // Summary is best-effort.
    }
}

std::string AIConvo::RunChat(const std::vector<llama_token>& all_tokens,
                             float temperature,
                             int   max_tokens) {
    // RunChat() is the "inference core" for a conversation turn.
    //
    // Inputs:
    // - all_tokens: the fully formatted prompt (already tokenized) for this turn.
    //              This is what BuildPrompt() produced (system + summary + recent window
    //              + assistant prefix), then AIModel::Tokenize(...) converted to token ids.
    // - temperature / max_tokens: sampling controls for the reply.
    //
    // Outputs / side effects:
    // - Returns the assistant reply as UTF-8 text.
    // - Advances the llama.cpp context state (KV cache) in m_conversation_ctx.
    // - Updates m_tokens_processed and m_cached_prompt_tokens to describe what the KV cache
    //   currently corresponds to (so the next turn can optionally reuse it).
    //
    // High-level steps:
    //  1) Validate that the prompt fits the context window.
    //  2) Ensure KV cache matches the prompt:
    //     - either clear + full decode, or
    //     - reuse KV and decode only the delta tokens if the prompt is append-only.
    //  3) Create a sampler chain (temperature + top-p + RNG).
    //  4) Generate up to max_tokens:
    //     - sample next token
    //     - append its text piece to reply
    //     - decode it to extend KV cache for the next step
    //  5) Free the sampler and return reply.

    // ── (1) Context overflow check ────────────────────────────────────────────
    if (static_cast<int>(all_tokens.size()) >= m_model.CtxSize()) {
        throw std::runtime_error(
            "AIConvo::RunChat: conversation is too long for the context window ("
            + std::to_string(all_tokens.size()) + " prompt tokens, context limit "
            + std::to_string(m_model.CtxSize()) + ")");
    }

    // ── (2) Prompt evaluation (full or delta) ─────────────────────────────────
    // llama.cpp builds/updates the KV cache when you call llama_decode(ctx, batch).
    // This is how the prompt becomes "resident" in attention memory.
    //
    // Correctness rule:
    // - If the prompt changed anywhere *before the end*, reusing old KV is unsafe.
    // - We only reuse KV when the new prompt token list has the previous prompt as
    //   an exact prefix (append-only extension).
    auto decode_tokens = [&](const llama_token* data, int32_t n) {
        if (n <= 0) return;
        llama_batch b = llama_batch_get_one(const_cast<llama_token*>(data), n);
        if (llama_decode(m_conversation_ctx, b) != 0) {
            throw std::runtime_error("AIConvo::RunChat: llama_decode failed during prompt evaluation");
        }
    };

    if (m_clear_kv_each_turn) {
        // Mode A (default): always rebuild KV from scratch for maximum correctness.
        ClearKvCache();
        decode_tokens(all_tokens.data(), static_cast<int32_t>(all_tokens.size()));
        m_tokens_processed = static_cast<int32_t>(all_tokens.size());
        m_cached_prompt_tokens = all_tokens;
    } else {
        // Mode B: try to keep KV across turns to avoid re-decoding the whole prompt.
        const bool have_prefix =
            !m_cached_prompt_tokens.empty() &&
            m_tokens_processed == static_cast<int32_t>(m_cached_prompt_tokens.size()) &&
            all_tokens.size() >= m_cached_prompt_tokens.size() &&
            std::equal(m_cached_prompt_tokens.begin(), m_cached_prompt_tokens.end(), all_tokens.begin());

        if (!have_prefix) {
            // Prompt was not append-only → clear and rebuild to stay correct.
            ClearKvCache();
            decode_tokens(all_tokens.data(), static_cast<int32_t>(all_tokens.size()));
        } else {
            // Prompt extends the previous prompt → decode only the new tail tokens.
            const std::size_t delta = all_tokens.size() - m_cached_prompt_tokens.size();
            if (delta > 0) {
                decode_tokens(all_tokens.data() + m_cached_prompt_tokens.size(), static_cast<int32_t>(delta));
            }
        }

        m_tokens_processed = static_cast<int32_t>(all_tokens.size());
        m_cached_prompt_tokens = all_tokens;
    }

    // ── (3) Sampler chain: temperature → top-p → draw ─────────────────────────
    // This chain transforms the model's next-token logits into a single sampled token.
    llama_sampler_chain_params sampler_chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_chain_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string reply;
    reply.reserve(static_cast<std::size_t>(max_tokens) * 4);

    // ── (4) Autoregressive generation loop ────────────────────────────────────
    for (int i = 0; i < max_tokens; ++i) {
        // Sample 1 token from the current context state.
        llama_token current_token = llama_sampler_sample(sampler, m_conversation_ctx, -1);

        const llama_vocab* vocab = llama_model_get_vocab(m_model.ModelPtr());
        if (llama_vocab_is_eog(vocab, current_token)) break;

        // Convert token id -> UTF-8 piece and append to the reply text.
        char piece[32];
        int  piece_length = llama_token_to_piece(
            vocab, current_token, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            reply.append(piece, static_cast<std::size_t>(piece_length));

        // Decode the token to advance the model state (this extends the KV cache).
        llama_batch next_batch = llama_batch_get_one(&current_token, 1);
        if (llama_decode(m_conversation_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error(
                "AIConvo::RunChat: llama_decode failed during generation");
        }
        ++m_tokens_processed;
    }

    // ── (5) Cleanup ───────────────────────────────────────────────────────────
    llama_sampler_free(sampler);
    return reply;
}

std::string AIConvo::MakeTitle(const std::string& first_msg) {
    const std::string title_prompt =
        "In 5 words or fewer, give a short title for this conversation.\n"
        "Message: " + first_msg + "\nTitle:";
    try {
        std::string raw_title = m_model.Generate(
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

std::string AIConvo::Chat(const std::string& message,
                          float temperature,
                          int   max_tokens) {
    if (IsBlank(message))
        throw std::invalid_argument("AIConvo::Chat: message must not be blank");
    if (temperature < 0.0f || temperature > 2.0f)
        throw std::invalid_argument(
            "AIConvo::Chat: temperature must be in [0.0, 2.0]; got "
            + std::to_string(temperature));
    if (max_tokens < 1)
        throw std::invalid_argument(
            "AIConvo::Chat: max_tokens must be >= 1; got "
            + std::to_string(max_tokens));

    // Remember whether this is the very first user turn (for title generation).
    bool is_first_user_message = true;
    for (const auto& history_entry : m_history)
        if (history_entry.role == Role::User) { is_first_user_message = false; break; }

    // Append the user message optimistically — roll back if inference fails.
    m_history.push_back({Role::User, message});

    try {
        // BuildPrompt() serialises ALL of m_history (system + every user/assistant
        // pair so far + the new user message + the assistant-turn prefix).
        // RunChat() always reprocesses this full prompt from scratch, so the model
        // sees the complete conversation history on every single turn.
        std::string full_prompt = BuildPrompt();
        auto        token_list  = m_model.Tokenize(full_prompt, /*add_bos=*/true);
        std::string reply       = RunChat(token_list, temperature, max_tokens);

        if (IsBlank(reply))
            throw std::runtime_error("AIConvo::Chat: model returned an empty response");

        m_history.push_back({Role::Assistant, reply});

        // Hidden summary update (best-effort, not user-visible).
        UpdateSummaryNoThrow();

        // Auto-generate a title from the first user message (best-effort).
        if (is_first_user_message && !m_title.has_value()) {
            try { m_title = MakeTitle(message); }
            catch (...) {}  // title is optional
        }

        return reply;

    } catch (...) {
        // Roll back the user message and clear the KV cache.
        m_history.pop_back();
        ClearKvCache();
        throw;
    }
}

void AIConvo::ClearHistory() noexcept {
    // Keep only the system message (always index 0).
    if (m_history.size() > 1)
        m_history.erase(m_history.begin() + 1, m_history.end());
    ClearKvCache();
    // Title is intentionally preserved — clearing turns does not rename the chat.
}

std::vector<Message> AIConvo::GetHistory() const {
    return m_history;  // return a copy so callers cannot mutate internal state
}

std::string AIConvo::SaveHistory(const std::string& path) {
    json json_document;
    json_document["title"] = m_title.has_value() ? json(*m_title) : json(nullptr);
    json_document["system_prompt_file"] = m_system_prompt_file.has_value() ? json(*m_system_prompt_file) : json(nullptr);
    json_document["recent_turns_window"] = m_recent_turns_window;
    json_document["summary"] = IsBlank(m_summary) ? json(nullptr) : json(m_summary);

    json json_messages = json::array();
    for (const auto& history_entry : m_history)
        json_messages.push_back({
            {"role",    RoleToStr(history_entry.role)},
            {"content", history_entry.content}
        });
    json_document["messages"] = json_messages;

    // Determine output path: use provided path, or auto-generate one.
    std::string output_file_path = path;
    if (IsBlank(output_file_path)) {
        std::string base_filename;
        if (m_title.has_value()) {
            base_filename = *m_title;
            for (char& c : base_filename)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
        } else {
            base_filename = "chat_" + NowStamp();
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

void AIConvo::SaveState(const std::string& path) {
    if (IsBlank(path))
        throw std::invalid_argument("AIConvo::SaveState: path must not be blank");
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvo::SaveState: null conversation context");

    const size_t state_size = llama_state_get_size(m_conversation_ctx);
    if (state_size == 0)
        throw std::runtime_error("AIConvo::SaveState: llama_state_get_size returned 0");

    std::vector<uint8_t> state_bytes(state_size);
    const size_t written = llama_state_get_data(m_conversation_ctx, state_bytes.data(), state_bytes.size());
    if (written == 0)
        throw std::runtime_error("AIConvo::SaveState: llama_state_get_data failed");

    state_bytes.resize(written);

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
        throw std::runtime_error("AIConvo::SaveState: cannot open \"" + path + "\" for writing");

    out.write(reinterpret_cast<const char*>(state_bytes.data()),
              static_cast<std::streamsize>(state_bytes.size()));
    if (!out)
        throw std::runtime_error("AIConvo::SaveState: write failed for \"" + path + "\"");
}

void AIConvo::LoadHistory(const std::string& path) {
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

        Role role = RoleFromStr(item["role"].get<std::string>());
        loaded_history.push_back({role, item["content"].get<std::string>()});
    }

    std::optional<std::string> loaded_title;
    if (json_document.contains("title") && json_document["title"].is_string())
        loaded_title = json_document["title"].get<std::string>();

    std::optional<std::string> loaded_system_prompt_file;
    if (json_document.contains("system_prompt_file") && json_document["system_prompt_file"].is_string())
        loaded_system_prompt_file = json_document["system_prompt_file"].get<std::string>();

    int loaded_recent_turns_window = m_recent_turns_window;
    if (json_document.contains("recent_turns_window") && json_document["recent_turns_window"].is_number_integer())
        loaded_recent_turns_window = json_document["recent_turns_window"].get<int>();

    std::string loaded_summary;
    if (json_document.contains("summary") && json_document["summary"].is_string())
        loaded_summary = json_document["summary"].get<std::string>();

    // Commit only after all validation succeeds.
    m_history = std::move(loaded_history);
    m_title   = std::move(loaded_title);
    m_system_prompt_file = std::move(loaded_system_prompt_file);
    m_recent_turns_window = loaded_recent_turns_window < 0 ? 0 : loaded_recent_turns_window;
    m_summary = std::move(loaded_summary);
    ClearKvCache();  // loaded history has no cached KV vectors yet
}

void AIConvo::LoadState(const std::string& path) {
    if (IsBlank(path))
        throw std::invalid_argument("AIConvo::LoadState: path must not be blank");
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvo::LoadState: null conversation context");

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        throw std::runtime_error("AIConvo::LoadState: cannot open \"" + path + "\" for reading");

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0)
        throw std::runtime_error("AIConvo::LoadState: empty state file \"" + path + "\"");
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> state_bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(state_bytes.data()),
            static_cast<std::streamsize>(state_bytes.size()));
    if (!in)
        throw std::runtime_error("AIConvo::LoadState: read failed for \"" + path + "\"");

    const size_t restored = llama_state_set_data(m_conversation_ctx, state_bytes.data(), state_bytes.size());
    if (restored == 0)
        throw std::runtime_error("AIConvo::LoadState: llama_state_set_data failed for \"" + path + "\"");
}

std::optional<std::string> AIConvo::GetTitle() const noexcept {
    return m_title;
}

void AIConvo::SetTitle(const std::string& title) {
    if (IsBlank(title))
        throw std::invalid_argument("AIConvo::SetTitle: title must not be blank");
    m_title = title;
}
