#pragma once

#include <cstdint>      // uint32_t
#include <memory>       // std::unique_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <vector>       // std::vector

namespace convo_manager {

using ModelId = uint32_t;
using ConvoId = uint32_t;

struct ConversationInfo {
  ModelId Model = 0;
  ConvoId Id = 0;
  std::optional<std::string> Title;
  std::optional<std::string> LastSavedPath;
  std::optional<std::string> LastStatePath;
  bool Dirty = false;
  bool Closed = false;
};

struct ModelInfo {
  ModelId Id = 0;
  std::string ModelPath;
  std::vector<ConversationInfo> Conversations;
  std::optional<ConvoId> ActiveConversation;
};

class ConvoManager {
public:
  ConvoManager();
  ~ConvoManager() noexcept;

  ConvoManager(const ConvoManager&) = delete;
  ConvoManager& operator=(const ConvoManager&) = delete;

  // ── Models ──────────────────────────────────────────────────────────────────
  ModelId AddModel(const std::string& model_path, int context_size = 4096, int thread_count = 4);
  std::vector<ModelInfo> ListModels() const;

  // ── Conversations ───────────────────────────────────────────────────────────
  ConvoId NewConversation(ModelId model_id,
                          const std::string& system_prompt,
                          std::optional<std::string> initial_title = std::nullopt);

  void SwitchActiveConversation(ModelId model_id, ConvoId convo_id);
  std::optional<std::pair<ModelId, ConvoId>> GetActive() const;

  ConversationInfo GetConversationInfo(ModelId model_id, ConvoId convo_id) const;
  std::vector<ConversationInfo> ListConversations(ModelId model_id) const;

  // Send a chat message to the currently active conversation for the given model.
  // Marks the conversation Dirty on success.
  std::string Chat(ModelId model_id, const std::string& user_message,
                   float temperature = 0.7f, int max_tokens = 2048);

  // ── Persistence ─────────────────────────────────────────────────────────────
  // Save one conversation. If path is empty, AIConvo chooses an auto filename.
  // Returns the written history path.
  //
  // Additionally writes a binary llama.cpp state snapshot (KV cache, etc.) to a
  // sidecar file: "<history_path>.state.bin".
  std::string Save(ModelId model_id, ConvoId convo_id, const std::string& path = "");

  // Save an entire session directory (models + conversations + llama state).
  //
  // This writes:
  // - <dir>/manifest.json  (models, parameters, conversation file paths)
  // - <dir>/convos/...     (per-conversation history JSON + state .bin)
  //
  // Throws on errors. For "never throws" persistence, call SaveAllNoThrow().
  void SaveSession(const std::string& dir);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace convo_manager

