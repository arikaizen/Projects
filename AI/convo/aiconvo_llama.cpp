#include "aiconvo_llama.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

AIConvoLlama::AIConvoLlama(AIModelLlama& model, const std::string& system_prompt)
    : AIConvo(model, system_prompt)
    , m_llama_model(model)
{
    // AIConvo base constructor already validates system_prompt.

    llama_context_params context_params  = llama_context_default_params();
    context_params.n_ctx                 = static_cast<uint32_t>(model.CtxSize());
    context_params.n_batch               = static_cast<uint32_t>(model.CtxSize());
    context_params.n_threads             = static_cast<uint32_t>(model.ThreadCount());
    context_params.n_threads_batch       = static_cast<uint32_t>(model.ThreadCount());

    m_conversation_ctx = llama_init_from_model(model.ModelPtr(), context_params);
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvoLlama: failed to create dedicated chat context");
}

AIConvoLlama::~AIConvoLlama() {
    if (m_conversation_ctx) { llama_free(m_conversation_ctx); m_conversation_ctx = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
// KV cache management
// ─────────────────────────────────────────────────────────────────────────────

void AIConvoLlama::ClearKvCache() noexcept {
    llama_memory_clear(llama_get_memory(m_conversation_ctx), /*data=*/true);
    m_tokens_processed = 0;
    m_cached_prompt_tokens.clear();
}

void AIConvoLlama::OnChatFailure() noexcept {
    ClearKvCache();
}

void AIConvoLlama::SetClearKvCacheEachTurn(bool clear) noexcept {
    if (m_clear_kv_each_turn == clear) return;
    m_clear_kv_each_turn = clear;
    ClearKvCache();
}

bool AIConvoLlama::GetClearKvCacheEachTurn() const noexcept {
    return m_clear_kv_each_turn;
}

// ─────────────────────────────────────────────────────────────────────────────
// FormatPrompt — convert vector<Message> to a llama-formatted prompt string
// ─────────────────────────────────────────────────────────────────────────────

std::string AIConvoLlama::FormatPrompt(const std::vector<Message>& messages) const {
    std::vector<std::string>        role_strings;
    std::vector<llama_chat_message> chat_message_array;

    role_strings.reserve(messages.size());
    chat_message_array.reserve(messages.size());

    for (const auto& message : messages) {
        role_strings.push_back(RoleToStr(message.role));
        chat_message_array.push_back({role_strings.back().c_str(), message.content.c_str()});
    }

    const char* chat_template = llama_model_chat_template(m_llama_model.ModelPtr(), nullptr);

    // Dry run to get required buffer size.
    int required_buffer_size = llama_chat_apply_template(
        chat_template,
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        nullptr, 0
    );

    if (required_buffer_size < 0) {
        // No built-in template — fall back to "role: content\n" plain text.
        std::ostringstream out;
        for (const auto& message : messages)
            out << RoleToStr(message.role) << ": " << message.content << "\n";
        out << "assistant: ";
        return out.str();
    }

    std::vector<char> formatted_buffer(
        static_cast<std::size_t>(required_buffer_size) + 1, '\0');
    llama_chat_apply_template(
        chat_template,
        chat_message_array.data(), static_cast<int>(chat_message_array.size()),
        /*add_ass=*/true,
        formatted_buffer.data(), static_cast<int>(formatted_buffer.size())
    );

    return {formatted_buffer.data(), static_cast<std::size_t>(required_buffer_size)};
}

// ─────────────────────────────────────────────────────────────────────────────
// GenerateReply — the core inference method called by AIConvo::Chat
// ─────────────────────────────────────────────────────────────────────────────

std::string AIConvoLlama::GenerateReply(const std::vector<Message>& prompt_messages,
                                        float temperature, int max_tokens) {
    const std::string prompt = FormatPrompt(prompt_messages);
    const auto all_tokens = m_llama_model.Tokenize(prompt, /*add_bos=*/true);

    // ── Context overflow check ────────────────────────────────────────────────
    if (static_cast<int>(all_tokens.size()) >= m_llama_model.CtxSize()) {
        throw std::runtime_error(
            "AIConvoLlama::GenerateReply: conversation is too long for the context window ("
            + std::to_string(all_tokens.size()) + " prompt tokens, context limit "
            + std::to_string(m_llama_model.CtxSize()) + ")");
    }

    // ── Prompt evaluation (full or delta decode) ──────────────────────────────
    auto decode_tokens = [&](const llama_token* data, int32_t n) {
        if (n <= 0) return;
        llama_batch b = llama_batch_get_one(const_cast<llama_token*>(data), n);
        if (llama_decode(m_conversation_ctx, b) != 0) {
            throw std::runtime_error(
                "AIConvoLlama::GenerateReply: llama_decode failed during prompt evaluation");
        }
    };

    if (m_clear_kv_each_turn) {
        ClearKvCache();
        decode_tokens(all_tokens.data(), static_cast<int32_t>(all_tokens.size()));
        m_tokens_processed = static_cast<int32_t>(all_tokens.size());
        m_cached_prompt_tokens = all_tokens;
    } else {
        const bool have_prefix =
            !m_cached_prompt_tokens.empty() &&
            m_tokens_processed == static_cast<int32_t>(m_cached_prompt_tokens.size()) &&
            all_tokens.size() >= m_cached_prompt_tokens.size() &&
            std::equal(m_cached_prompt_tokens.begin(), m_cached_prompt_tokens.end(),
                       all_tokens.begin());

        if (!have_prefix) {
            ClearKvCache();
            decode_tokens(all_tokens.data(), static_cast<int32_t>(all_tokens.size()));
        } else {
            const std::size_t delta = all_tokens.size() - m_cached_prompt_tokens.size();
            if (delta > 0) {
                decode_tokens(all_tokens.data() + m_cached_prompt_tokens.size(),
                              static_cast<int32_t>(delta));
            }
        }

        m_tokens_processed = static_cast<int32_t>(all_tokens.size());
        m_cached_prompt_tokens = all_tokens;
    }

    // ── Sampler chain: temperature → top-p → draw ────────────────────────────
    llama_sampler_chain_params sampler_chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler = llama_sampler_chain_init(sampler_chain_params);
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::string reply;
    reply.reserve(static_cast<std::size_t>(max_tokens) * 4);

    const llama_vocab* vocab = llama_model_get_vocab(m_llama_model.ModelPtr());

    // ── Autoregressive generation loop ───────────────────────────────────────
    llama_token new_id;
    for (int i = 0; i < max_tokens; ++i) {
        new_id = llama_sampler_sample(sampler, m_conversation_ctx, -1);

        if (llama_vocab_is_eog(vocab, new_id)) break;

        char piece[32];
        int  piece_length = llama_token_to_piece(
            vocab, new_id, piece, sizeof piece, 0, false);
        if (piece_length > 0)
            reply.append(piece, static_cast<std::size_t>(piece_length));

        llama_batch next_batch = llama_batch_get_one(&new_id, 1);
        if (llama_decode(m_conversation_ctx, next_batch) != 0) {
            llama_sampler_free(sampler);
            throw std::runtime_error(
                "AIConvoLlama::GenerateReply: llama_decode failed during generation");
        }
        ++m_tokens_processed;
    }

    llama_sampler_free(sampler);
    return reply;
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveState / LoadState
// ─────────────────────────────────────────────────────────────────────────────

void AIConvoLlama::SaveState(const std::string& path) {
    if (path.empty())
        throw std::invalid_argument("AIConvoLlama::SaveState: path must not be blank");
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvoLlama::SaveState: null conversation context");

    const size_t state_size = llama_state_get_size(m_conversation_ctx);
    if (state_size == 0)
        throw std::runtime_error("AIConvoLlama::SaveState: llama_state_get_size returned 0");

    std::vector<uint8_t> state_bytes(state_size);
    const size_t written = llama_state_get_data(
        m_conversation_ctx, state_bytes.data(), state_bytes.size());
    if (written == 0)
        throw std::runtime_error("AIConvoLlama::SaveState: llama_state_get_data failed");

    state_bytes.resize(written);

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
        throw std::runtime_error(
            "AIConvoLlama::SaveState: cannot open \"" + path + "\" for writing");

    out.write(reinterpret_cast<const char*>(state_bytes.data()),
              static_cast<std::streamsize>(state_bytes.size()));
    if (!out)
        throw std::runtime_error(
            "AIConvoLlama::SaveState: write failed for \"" + path + "\"");
}

void AIConvoLlama::LoadState(const std::string& path) {
    if (path.empty())
        throw std::invalid_argument("AIConvoLlama::LoadState: path must not be blank");
    if (!m_conversation_ctx)
        throw std::runtime_error("AIConvoLlama::LoadState: null conversation context");

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        throw std::runtime_error(
            "AIConvoLlama::LoadState: cannot open \"" + path + "\" for reading");

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size <= 0)
        throw std::runtime_error(
            "AIConvoLlama::LoadState: empty state file \"" + path + "\"");
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> state_bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(state_bytes.data()),
            static_cast<std::streamsize>(state_bytes.size()));
    if (!in)
        throw std::runtime_error(
            "AIConvoLlama::LoadState: read failed for \"" + path + "\"");

    const size_t restored = llama_state_set_data(
        m_conversation_ctx, state_bytes.data(), state_bytes.size());
    if (restored == 0)
        throw std::runtime_error(
            "AIConvoLlama::LoadState: llama_state_set_data failed for \"" + path + "\"");
}
