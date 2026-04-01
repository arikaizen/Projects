/**
 * AI_convo.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Implementation of AI_convo.hpp.
 *
 * Design notes
 * ────────────
 * AIModel
 *   Owns a llama_model* and a general-purpose llama_context*.  All stateless
 *   calls (generate, embed, similarity, search) share this single context.
 *   _decode() clears the KV cache at the start of every call so each generate()
 *   is fully independent.  Embedding is handled by a short-lived second context
 *   configured for mean-pool embedding output.
 *
 * AIConvo
 *   Owns a separate llama_context* (_ctx) whose KV cache accumulates across
 *   turns.  On each chat() call _run_chat() feeds only the tokens that the
 *   model has not seen yet (all_tokens[_n_past..]) into llama_decode, then
 *   auto-regressively samples the reply.  This avoids re-processing the entire
 *   history on every turn.
 *
 *   History rollback: if inference throws after the user message was appended,
 *   the message is popped and _flush_kv() is called so the next chat() starts
 *   from a coherent state.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "AI_convo.hpp"

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
float vec_norm(const std::vector<float>& v) {
    float sq = std::inner_product(v.begin(), v.end(), v.begin(), 0.0f);
    return std::sqrt(sq);
}

/** Cosine similarity of two equal-length float vectors.
 *  Returns 0 when either vector has zero magnitude. */
float cosine_sim(const std::vector<float>& a, const std::vector<float>& b) {
    assert(a.size() == b.size());
    float dot   = std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
    float na    = vec_norm(a);
    float nb    = vec_norm(b);
    if (na == 0.0f || nb == 0.0f) return 0.0f;
    return dot / (na * nb);
}

/** ISO-8601-like timestamp string (YYYY-MM-DD_HH-MM-SS) for auto filenames. */
std::string now_stamp() {
    auto tp = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d_%H-%M-%S", &tm);
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

AIModel::AIModel(const std::string& model_path, int ctx_size, int n_threads)
    : _model(nullptr), _ctx(nullptr), _ctx_size(ctx_size), _n_threads(n_threads)
{
    // Initialize the global llama backend (ref-counted; safe to call multiple times).
    llama_backend_init();

    // ── Load model weights ────────────────────────────────────────────────────
    llama_model_params mp = llama_model_default_params();
    _model = llama_model_load_from_file(model_path.c_str(), mp);
    if (!_model)
        throw std::runtime_error("AIModel: failed to load model from \"" + model_path + "\"");

    // ── Create inference context ──────────────────────────────────────────────
    llama_context_params cp    = llama_context_default_params();
    cp.n_ctx                   = static_cast<uint32_t>(ctx_size);
    cp.n_threads               = static_cast<uint32_t>(n_threads);
    cp.n_threads_batch         = static_cast<uint32_t>(n_threads);

    _ctx = llama_new_context_with_model(_model, cp);
    if (!_ctx) {
        llama_model_free(_model);
        _model = nullptr;
        throw std::runtime_error("AIModel: failed to create inference context for \"" + model_path + "\"");
    }
}

AIModel::~AIModel() noexcept {
    if (_ctx)   { llama_free(_ctx);        _ctx   = nullptr; }
    if (_model) { llama_model_free(_model); _model = nullptr; }
    llama_backend_free();
}

AIModel::AIModel(AIModel&& o) noexcept
    : _model(o._model)
    , _ctx(o._ctx)
    , _ctx_size(o._ctx_size)
    , _n_threads(o._n_threads)
    , _embd_cache(std::move(o._embd_cache))
{
    o._model = nullptr;
    o._ctx   = nullptr;
}

AIModel& AIModel::operator=(AIModel&& o) noexcept {
    if (this != &o) {
        if (_ctx)   llama_free(_ctx);
        if (_model) llama_model_free(_model);

        _model      = o._model;
        _ctx        = o._ctx;
        _ctx_size   = o._ctx_size;
        _n_threads  = o._n_threads;
        _embd_cache = std::move(o._embd_cache);

        o._model = nullptr;
        o._ctx   = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIModel — private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<llama_token> AIModel::_tokenize(const std::string& text, bool add_bos) const {
    // First call with a null buffer returns (negative) required count.
    int n = llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        nullptr, 0,
        add_bos,
        /*special=*/false
    );
    if (n < 0) n = -n;

    std::vector<llama_token> toks(static_cast<std::size_t>(n));
    llama_tokenize(
        _model,
        text.c_str(), static_cast<int32_t>(text.size()),
        toks.data(), static_cast<int32_t>(toks.size()),
        add_bos,
        /*special=*/false
    );
    return toks;
}

std::string AIModel::_decode(const std::vector<llama_token>& tokens,
                              float temp, int max_tokens) const {
    // Each generate() is stateless — clear any KV vectors left by a previous call.
    llama_memory_clear(llama_get_memory(_ctx), /*data=*/true);

    // Feed the full prompt into the context in one batch.
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(tokens.data()),
        static_cast<int32_t>(tokens.size())
    );
    if (llama_decode(_ctx, prompt_batch) != 0)
        throw std::runtime_error("AIModel::_decode: llama_decode failed during prompt evaluation");

    // Build a sampler chain: temperature → top-p (nucleus) → distribution draw.
    llama_sampler_chain_params scp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(scp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temp));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string out;
    out.reserve(static_cast<std::size_t>(max_tokens) * 4);

    int32_t n_past = static_cast<int32_t>(tokens.size());

    for (int i = 0; i < max_tokens; ++i) {
        llama_token tok = llama_sampler_sample(sampler, _ctx, -1);

        if (llama_token_is_eog(_model, tok)) break;

        // Decode the token id to its UTF-8 string piece (up to 32 bytes).
        char piece[32];
        int  plen = llama_token_to_piece(_model, tok, piece, sizeof piece, 0, false);
        if (plen > 0) out.append(piece, static_cast<std::size_t>(plen));

        // Feed the newly generated token back for the next step.
        llama_batch next = llama_batch_get_one(&tok, 1);
        if (llama_decode(_ctx, next) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("AIModel::_decode: llama_decode failed during generation");
        }
        ++n_past;
    }

    llama_sampler_free(sampler);
    return out;
}

std::vector<float> AIModel::_raw_embed(const std::string& text) const {
    // Create a short-lived context configured for mean-pooled embeddings.
    llama_context_params ep  = llama_context_default_params();
    ep.n_ctx                 = 512;
    ep.n_threads             = static_cast<uint32_t>(_n_threads);
    ep.embeddings            = true;
    ep.pooling_type          = LLAMA_POOLING_TYPE_MEAN;

    llama_context* ectx = llama_new_context_with_model(_model, ep);
    if (!ectx)
        throw std::runtime_error("AIModel::_raw_embed: failed to create embedding context");

    // Embed without BOS — it disrupts mean pooling.
    std::vector<llama_token> toks = _tokenize(text, /*add_bos=*/false);

    llama_batch batch = llama_batch_get_one(toks.data(), static_cast<int32_t>(toks.size()));
    if (llama_decode(ectx, batch) != 0) {
        llama_free(ectx);
        throw std::runtime_error("AIModel::_raw_embed: llama_decode failed");
    }

    int          n_embd  = llama_n_embd(_model);
    const float* eptr    = llama_get_embeddings_seq(ectx, 0);
    if (!eptr) {
        llama_free(ectx);
        throw std::runtime_error("AIModel::_raw_embed: no embedding output "
                                 "(does this model support embeddings?)");
    }

    std::vector<float> vec(eptr, eptr + n_embd);
    llama_free(ectx);
    return vec;
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

    auto tokens = _tokenize(prompt, /*add_bos=*/true);
    auto result = _decode(tokens, temperature, max_tokens);

    if (is_blank(result))
        throw std::runtime_error("AIModel::generate: model returned an empty response");

    return result;
}

std::vector<float> AIModel::embed(const std::string& text, bool use_cache) {
    if (is_blank(text))
        throw std::invalid_argument("AIModel::embed: text must not be blank");

    // Return cached vector when available.
    if (use_cache) {
        auto it = _embd_cache.find(text);
        if (it != _embd_cache.end()) return it->second;
    }

    auto vec = _raw_embed(text);

    if (vec_norm(vec) == 0.0f)
        throw std::runtime_error(
            "AIModel::embed: embedding has zero magnitude "
            "(model may not support embeddings)");

    if (use_cache)
        _embd_cache[text] = vec;

    return vec;
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

    auto query_vec = embed(query);

    std::vector<std::pair<float, std::string>> ranked;
    ranked.reserve(texts.size());
    for (std::size_t i = 0; i < texts.size(); ++i) {
        float score = cosine_sim(query_vec, embed(texts[i]));
        ranked.emplace_back(score, labels[i]);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& x, const auto& y) { return x.first > y.first; });

    int take = std::min(top_n, static_cast<int>(ranked.size()));
    return {ranked.begin(), ranked.begin() + take};
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIConvo::AIConvo(AIModel& model, const std::string& system_prompt)
    : _model(model)
    , _sys_prompt(system_prompt)
    , _ctx(nullptr)
    , _n_past(0)
{
    if (is_blank(system_prompt))
        throw std::invalid_argument("AIConvo: system_prompt must not be blank");

    // Allocate a dedicated context whose KV cache belongs to this conversation.
    llama_context_params cp  = llama_context_default_params();
    cp.n_ctx                 = static_cast<uint32_t>(model._ctx_size);
    cp.n_threads             = static_cast<uint32_t>(model._n_threads);
    cp.n_threads_batch       = static_cast<uint32_t>(model._n_threads);

    _ctx = llama_new_context_with_model(model.model_ptr(), cp);
    if (!_ctx)
        throw std::runtime_error("AIConvo: failed to create dedicated chat context");

    // Seed the history with the system message.
    _history.push_back({Role::System, system_prompt});
}

AIConvo::~AIConvo() noexcept {
    if (_ctx) { llama_free(_ctx); _ctx = nullptr; }
}

AIConvo::AIConvo(AIConvo&& o) noexcept
    : _model(o._model)
    , _sys_prompt(std::move(o._sys_prompt))
    , _history(std::move(o._history))
    , _title(std::move(o._title))
    , _ctx(o._ctx)
    , _n_past(o._n_past)
{
    o._ctx    = nullptr;
    o._n_past = 0;
}

AIConvo& AIConvo::operator=(AIConvo&& o) noexcept {
    if (this != &o) {
        if (_ctx) llama_free(_ctx);

        // _model is a reference — both sides must already refer to the same object.
        _sys_prompt = std::move(o._sys_prompt);
        _history    = std::move(o._history);
        _title      = std::move(o._title);
        _ctx        = o._ctx;
        _n_past     = o._n_past;

        o._ctx    = nullptr;
        o._n_past = 0;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// AIConvo — private helpers
// ─────────────────────────────────────────────────────────────────────────────

void AIConvo::_flush_kv() noexcept {
    llama_memory_clear(llama_get_memory(_ctx), /*data=*/true);
    _n_past = 0;
}

std::string AIConvo::_build_prompt() const {
    // Build a C array of llama_chat_message structs from our history.
    // We keep role strings alive in a parallel vector.
    std::vector<std::string>        role_strs;
    std::vector<llama_chat_message> msgs;
    role_strs.reserve(_history.size());
    msgs.reserve(_history.size());

    for (const auto& m : _history) {
        role_strs.push_back(role_to_str(m.role));
        msgs.push_back({role_strs.back().c_str(), m.content.c_str()});
    }

    // Dry-run: pass null buffer to get the required byte count.
    int needed = llama_chat_apply_template(
        _model.model_ptr(),
        nullptr,         // use the model's built-in template
        msgs.data(), static_cast<int>(msgs.size()),
        /*add_ass=*/true,
        nullptr, 0
    );

    if (needed < 0) {
        // Model has no built-in template; fall back to a plain-text format.
        std::ostringstream oss;
        for (const auto& m : _history)
            oss << role_to_str(m.role) << ": " << m.content << "\n";
        oss << "assistant: ";
        return oss.str();
    }

    std::vector<char> buf(static_cast<std::size_t>(needed) + 1, '\0');
    llama_chat_apply_template(
        _model.model_ptr(),
        nullptr,
        msgs.data(), static_cast<int>(msgs.size()),
        /*add_ass=*/true,
        buf.data(), static_cast<int>(buf.size())
    );

    return {buf.data(), static_cast<std::size_t>(needed)};
}

std::string AIConvo::_run_chat(const std::vector<llama_token>& all_tokens,
                                float temp, int max_tokens) {
    // Compute how many tokens the model has not yet seen.
    int32_t n_new = static_cast<int32_t>(all_tokens.size()) - _n_past;

    // Guard: if _n_past is somehow ahead of the token sequence, reset and reprocess.
    if (n_new <= 0) {
        _flush_kv();
        n_new = static_cast<int32_t>(all_tokens.size());
    }

    // Feed only the new suffix into the KV cache.
    llama_batch prompt_batch = llama_batch_get_one(
        const_cast<llama_token*>(all_tokens.data()) + _n_past,
        n_new
    );
    if (llama_decode(_ctx, prompt_batch) != 0)
        throw std::runtime_error("AIConvo::_run_chat: llama_decode failed during prompt evaluation");

    _n_past += n_new;

    // Sampler chain: temperature → top-p → draw.
    llama_sampler_chain_params scp = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(scp);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temp));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string reply;
    reply.reserve(static_cast<std::size_t>(max_tokens) * 4);

    for (int i = 0; i < max_tokens; ++i) {
        llama_token tok = llama_sampler_sample(sampler, _ctx, -1);

        if (llama_token_is_eog(_model.model_ptr(), tok)) break;

        char piece[32];
        int  plen = llama_token_to_piece(_model.model_ptr(), tok, piece, sizeof piece, 0, false);
        if (plen > 0) reply.append(piece, static_cast<std::size_t>(plen));

        llama_batch next = llama_batch_get_one(&tok, 1);
        if (llama_decode(_ctx, next) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error("AIConvo::_run_chat: llama_decode failed during generation");
        }
        ++_n_past;
    }

    llama_sampler_free(sampler);
    return reply;
}

std::string AIConvo::_make_title(const std::string& first_msg) {
    const std::string prompt =
        "In 5 words or fewer, give a short title for this conversation.\n"
        "Message: " + first_msg + "\nTitle:";
    try {
        std::string raw = _model.generate(prompt, /*temperature=*/0.3f, /*max_tokens=*/20);
        auto start = raw.find_first_not_of(" \t\n\r");
        auto end   = raw.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return first_msg.substr(0, 40);
        return raw.substr(start, end - start + 1);
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
    bool first_user = true;
    for (const auto& m : _history)
        if (m.role == Role::User) { first_user = false; break; }

    // Append the user message optimistically — roll back if inference fails.
    _history.push_back({Role::User, message});

    try {
        std::string prompt = _build_prompt();
        auto tokens        = _model._tokenize(prompt, /*add_bos=*/true);
        std::string reply  = _run_chat(tokens, temperature, max_tokens);

        if (is_blank(reply))
            throw std::runtime_error("AIConvo::chat: model returned an empty response");

        _history.push_back({Role::Assistant, reply});

        // Auto-generate a title from the first user message (best-effort).
        if (first_user && !_title.has_value()) {
            try { _title = _make_title(message); }
            catch (...) {}  // title is optional
        }

        return reply;

    } catch (...) {
        // Roll back the user message and flush the KV cache.
        _history.pop_back();
        _flush_kv();
        throw;
    }
}

void AIConvo::clear_history() noexcept {
    // Keep only the system message (always index 0).
    if (_history.size() > 1)
        _history.erase(_history.begin() + 1, _history.end());
    _flush_kv();
    // Title is intentionally preserved — clearing turns does not rename the chat.
}

std::vector<Message> AIConvo::get_history() const {
    return _history;  // return a copy so callers cannot mutate internal state
}

std::string AIConvo::save_history(const std::string& path) {
    json doc;
    doc["title"] = _title.has_value() ? json(*_title) : json(nullptr);

    json msgs = json::array();
    for (const auto& m : _history)
        msgs.push_back({{"role", role_to_str(m.role)}, {"content", m.content}});
    doc["messages"] = msgs;

    // Determine output path: use provided path, or auto-generate one.
    std::string out = path;
    if (is_blank(out)) {
        std::string base;
        if (_title.has_value()) {
            base = *_title;
            for (char& c : base)
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
        } else {
            base = "chat_" + now_stamp();
        }
        out = base + ".json";
    }

    std::ofstream ofs(out);
    if (!ofs.is_open())
        throw std::runtime_error("AIConvo::save_history: cannot open \"" + out + "\" for writing");

    ofs << doc.dump(2);
    if (!ofs)
        throw std::runtime_error("AIConvo::save_history: write failed for \"" + out + "\"");

    return out;
}

void AIConvo::load_history(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open())
        throw std::runtime_error("AIConvo::load_history: cannot open \"" + path + "\"");

    json doc;
    try {
        ifs >> doc;
    } catch (const json::parse_error& e) {
        throw std::invalid_argument(
            std::string("AIConvo::load_history: malformed JSON in \"") + path + "\": " + e.what());
    }

    if (!doc.is_object())
        throw std::invalid_argument("AIConvo::load_history: expected a JSON object at top level");
    if (!doc.contains("messages") || !doc["messages"].is_array())
        throw std::invalid_argument("AIConvo::load_history: JSON must contain a \"messages\" array");

    std::vector<Message> new_history;
    for (const auto& item : doc["messages"]) {
        if (!item.contains("role") || !item["role"].is_string())
            throw std::invalid_argument("AIConvo::load_history: each message must have a string \"role\"");
        if (!item.contains("content") || !item["content"].is_string())
            throw std::invalid_argument("AIConvo::load_history: each message must have a string \"content\"");

        Role role = role_from_str(item["role"].get<std::string>());
        new_history.push_back({role, item["content"].get<std::string>()});
    }

    std::optional<std::string> new_title;
    if (doc.contains("title") && doc["title"].is_string())
        new_title = doc["title"].get<std::string>();

    // Commit only after all validation succeeds.
    _history = std::move(new_history);
    _title   = std::move(new_title);
    _flush_kv();  // loaded history has no cached KV vectors yet
}

std::optional<std::string> AIConvo::get_title() const noexcept {
    return _title;
}

void AIConvo::set_title(const std::string& title) {
    if (is_blank(title))
        throw std::invalid_argument("AIConvo::set_title: title must not be blank");
    _title = title;
}
