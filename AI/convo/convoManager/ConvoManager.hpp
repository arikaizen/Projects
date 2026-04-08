#pragma once

#include "convo.hpp"

#include <cstdint>      // uint32_t
#include <map>          // std::map
#include <memory>       // std::unique_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <vector>       // std::vector

namespace convo_manager {

using ModelId = uint32_t;
using ConvoId = uint32_t;

struct ConversationInfo {
  ModelId Model = 0;
  ConvoId Id = 0;
  std::optional<std::string> Title;
  std::optional<std::string> LastSavedPath;
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
  ConvoManager() = default;
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
  // Returns the written path.
  std::string Save(ModelId model_id, ConvoId convo_id, const std::string& path = "");

  // Save all conversations best-effort (never throws).
  void SaveAllNoThrow() noexcept;

  // Close a conversation (save best-effort, then mark Closed).
  void Close(ModelId model_id, ConvoId convo_id);

private:
  struct ConvoEntry {
    ConvoId Id = 0;
    std::unique_ptr<AIConvo> Convo;
    std::optional<std::string> LastSavedPath;
    bool Dirty = false;
    bool Closed = false;
  };

  struct ModelEntry {
    ModelId Id = 0;
    std::string ModelPath;
    std::unique_ptr<AIModel> Model;
    std::map<ConvoId, ConvoEntry> Convos;
    ConvoId NextConvoId = 1;
    std::optional<ConvoId> Active;
  };

  ModelEntry& RequireModel(ModelId model_id);
  const ModelEntry& RequireModel(ModelId model_id) const;
  ConvoEntry& RequireConvo(ModelEntry& model, ConvoId convo_id);
  const ConvoEntry& RequireConvo(const ModelEntry& model, ConvoId convo_id) const;

  std::map<ModelId, ModelEntry> _models;
  ModelId _next_model_id = 1;
};

} // namespace convo_manager

