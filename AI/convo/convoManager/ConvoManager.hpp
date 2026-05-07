#pragma once

#include <cstdint>      // uint32_t
#include <memory>       // std::unique_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <vector>       // std::vector
#include <mutex>        // std::mutex

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

struct BatchRequest {
  ModelId     model_id    = 0;
  ConvoId     convo_id    = 0;
  std::string message;
  float       temperature = 0.7f;
  int         max_tokens  = 2048;
};

struct BatchResult {
  ModelId     model_id = 0;
  ConvoId     convo_id = 0;
  std::string reply;
  std::string error;   // non-empty if inference failed
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

  // Send a message to the active conversation for this model.
  std::string Chat(ModelId model_id, const std::string& user_message,
                   float temperature = 0.7f, int max_tokens = 2048);

  // Send a message directly to a specific conversation (does not change active).
  std::string Chat(ModelId model_id, ConvoId convo_id,
                   const std::string& user_message,
                   float temperature = 0.7f, int max_tokens = 2048);

  // Run multiple conversations in parallel (one thread per request).
  // Each (model_id, convo_id) pair must be unique within the batch.
  // Never throws; per-request errors are reported in BatchResult::error.
  std::vector<BatchResult> ChatBatch(const std::vector<BatchRequest>& requests);

  // ── Per-conversation configuration ──────────────────────────────────────────
  void SetAgentConfig(ModelId model_id, ConvoId convo_id,
                      int recent_turns_window, bool clear_kv_each_turn);

  // ── Persistence ─────────────────────────────────────────────────────────────
  // Save one conversation. Returns the written history path.
  // Also writes KV cache to "<path>.state.bin".
  std::string Save(ModelId model_id, ConvoId convo_id, const std::string& path = "");

  // Restore history and KV state for a specific conversation.
  void LoadConvoHistory(ModelId model_id, ConvoId convo_id, const std::string& path);
  void LoadConvoState  (ModelId model_id, ConvoId convo_id, const std::string& path);

  // Save all dirty conversations; never throws (safe for signal handlers).
  void SaveAllNoThrow() noexcept;

  // Mark a conversation closed (saves best-effort first).
  void Close(ModelId model_id, ConvoId convo_id);

  // Save an entire session directory with manifest.json.
  void SaveSession(const std::string& dir);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace convo_manager

