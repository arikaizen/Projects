/**
 * llama_functions.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Implementation of llama_functions.h.
 *
 * Every method forwards to the AIModel / AIConvo objects held internally.
 * There are no direct llama.cpp calls here — all inference happens inside
 * AI_convo.cpp.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "llama_functions.h"

// ─────────────────────────────────────────────────────────────────────────────
// LlamaModel
// ─────────────────────────────────────────────────────────────────────────────

LlamaModel::LlamaModel(const std::string& model_path, int ctx_size, int n_threads)
    : _impl(std::make_unique<AIModel>(model_path, ctx_size, n_threads))
{}

LlamaModel::~LlamaModel() noexcept = default;

// Move: transfer the unique_ptr; the pointed-to AIModel stays at the same
// heap address, so any LlamaChat that holds a reference to it remains valid.
LlamaModel::LlamaModel(LlamaModel&&) noexcept            = default;
LlamaModel& LlamaModel::operator=(LlamaModel&&) noexcept = default;

// ── Inference delegation ──────────────────────────────────────────────────────

std::string LlamaModel::generate(const std::string& prompt,
                                  float temperature, int max_tokens) const {
    return _impl->generate(prompt, temperature, max_tokens);
}

std::vector<float> LlamaModel::embed(const std::string& text, bool use_cache) {
    return _impl->embed(text, use_cache);
}

float LlamaModel::similarity(const std::string& text_a, const std::string& text_b) {
    return _impl->similarity(text_a, text_b);
}

std::vector<std::pair<float, std::string>>
LlamaModel::search(const std::string&              query,
                   const std::vector<std::string>& labels,
                   const std::vector<std::string>& texts,
                   int top_n) {
    return _impl->search(query, labels, texts, top_n);
}

void LlamaModel::clear_cache() noexcept {
    _impl->clear_embed_cache();
}

AIModel& LlamaModel::ai_model() noexcept {
    return *_impl;
}

// ─────────────────────────────────────────────────────────────────────────────
// LlamaChat
// ─────────────────────────────────────────────────────────────────────────────

LlamaChat::LlamaChat(LlamaModel& model, const std::string& system_prompt)
    : _impl(std::make_unique<AIConvo>(model.ai_model(), system_prompt))
{}

LlamaChat::~LlamaChat() noexcept = default;

LlamaChat::LlamaChat(LlamaChat&&) noexcept            = default;
LlamaChat& LlamaChat::operator=(LlamaChat&&) noexcept = default;

// ── Conversation delegation ───────────────────────────────────────────────────

std::string LlamaChat::chat(const std::string& message,
                             float temperature, int max_tokens) {
    return _impl->chat(message, temperature, max_tokens);
}

void LlamaChat::clear_history() noexcept {
    _impl->clear_history();
}

std::vector<Message> LlamaChat::get_history() const {
    return _impl->get_history();
}

std::string LlamaChat::save_history(const std::string& filepath) {
    return _impl->save_history(filepath);
}

void LlamaChat::load_history(const std::string& filepath) {
    _impl->load_history(filepath);
}

std::optional<std::string> LlamaChat::get_title() const noexcept {
    return _impl->get_title();
}

void LlamaChat::set_title(const std::string& title) {
    _impl->set_title(title);
}
