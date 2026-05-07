#include "ConvoManager.hpp"

#include "convo.hpp"

#include <filesystem>  // std::filesystem::path, create_directories
#include <fstream>     // std::ifstream, std::ofstream
#include <map>       // std::map
#include <memory>    // std::make_unique
#include <nlohmann/json.hpp>
#include <stdexcept>  // std::invalid_argument, std::runtime_error
#include <utility>    // std::move

namespace convo_manager {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string SidecarStatePathForHistoryPath(const std::string& history_path) {
  return history_path + ".state.bin";
}

struct ConvoEntry {
  ConvoId Id = 0;
  std::unique_ptr<AIConvo> Convo;
  std::optional<std::string> LastSavedPath;
  std::optional<std::string> LastStatePath;
  bool Dirty = false;
  bool Closed = false;
};

struct ModelEntry {
  ModelId Id = 0;
  std::string ModelPath;
  int ContextSize = 4096;
  int ThreadCount = 4;
  std::unique_ptr<AIModel> Model;
  std::map<ConvoId, ConvoEntry> Convos;
  ConvoId NextConvoId = 1;
  std::optional<ConvoId> Active;
};

ModelEntry& RequireModel(std::map<ModelId, ModelEntry>& models, ModelId model_id) {
  auto it = models.find(model_id);
  if (it == models.end()) 
  {
    throw std::invalid_argument("ConvoManager: unknown model id");
  }
  return it->second;
}

ConvoEntry& RequireConvo(ModelEntry& model, ConvoId convo_id) {
  auto it = model.Convos.find(convo_id);
  if (it == model.Convos.end()) {
    throw std::invalid_argument("ConvoManager: unknown conversation id");
  }
  return it->second;
}

const ConvoEntry& RequireConvo(const ModelEntry& model, ConvoId convo_id) {
  auto it = model.Convos.find(convo_id);
  if (it == model.Convos.end()) {
    throw std::invalid_argument("ConvoManager: unknown conversation id");
  }
  return it->second;
}

} // namespace

struct ConvoManager::Impl {
  std::map<ModelId, ModelEntry> Models;
  ModelId NextModelId = 1;
};

ConvoManager::ConvoManager()
  : m_impl(std::make_unique<Impl>()) {}

ConvoManager::~ConvoManager() noexcept { 
  
}

ModelId ConvoManager::AddModel(const std::string& model_path, int context_size, int thread_count) {
  if (model_path.empty()) {
    throw std::invalid_argument("ConvoManager::AddModel: model_path must not be empty");
  }

  ModelEntry entry;
  entry.Id = m_impl->NextModelId++;
  entry.ModelPath = model_path;
  entry.ContextSize = context_size;
  entry.ThreadCount = thread_count;
  entry.Model = std::make_unique<AIModel>(model_path, context_size, thread_count);
  m_impl->Models.emplace(entry.Id, std::move(entry));
  return entry.Id;
}

std::vector<ModelInfo> ConvoManager::ListModels() const {
  std::vector<ModelInfo> out;
  out.reserve(m_impl->Models.size());

  for (const auto& [id, model] : m_impl->Models) {
    ModelInfo mi;
    mi.Id = id;
    mi.ModelPath = model.ModelPath;
    mi.ActiveConversation = model.Active;
    for (const auto& [cid, convo] : model.Convos) {
      (void)cid;
      ConversationInfo info;
      info.Model = id;
      info.Id = convo.Id;
      info.Title = convo.Convo ? convo.Convo->GetTitle() : std::nullopt;
      info.LastSavedPath = convo.LastSavedPath;
      info.LastStatePath = convo.LastStatePath;
      info.Dirty = convo.Dirty;
      info.Closed = convo.Closed;
      mi.Conversations.push_back(std::move(info));
    }
    out.push_back(std::move(mi));
  }
  return out;
}

ConvoId ConvoManager::NewConversation(ModelId model_id,
                                      const std::string& system_prompt,
                                      std::optional<std::string> initial_title) {
  auto& model = RequireModel(m_impl->Models, model_id);

  ConvoEntry e;
  e.Id = ++model.NextConvoId;
  e.Convo = std::make_unique<AIConvo>(*model.Model, system_prompt);
  if (initial_title && !initial_title->empty()) {
    e.Convo->SetTitle(*initial_title);
  }
  model.Convos.emplace(e.Id, std::move(e));
  model.Active = model.Active.value_or(e.Id);
  return e.Id;
}

void ConvoManager::SwitchActiveConversation(ModelId model_id, ConvoId convo_id) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (convo.Closed) {
    throw std::invalid_argument("ConvoManager::SwitchActiveConversation: conversation is closed");
  }
  model.Active = convo_id;
}

std::optional<std::pair<ModelId, ConvoId>> ConvoManager::GetActive() const {
  for (const auto& [mid, model] : m_impl->Models) {
    if (model.Active.has_value()) {
      return std::make_pair(mid, model.Active.value());
    }
  }
  return std::nullopt;
}

ConversationInfo ConvoManager::GetConversationInfo(ModelId model_id, ConvoId convo_id) const {
  const auto& model = RequireModel(m_impl->Models, model_id);
  const auto& convo = RequireConvo(model, convo_id);
  ConversationInfo info;
  info.Model = model_id;
  info.Id = convo.Id;
  info.Title = convo.Convo ? convo.Convo->GetTitle() : std::nullopt;
  info.LastSavedPath = convo.LastSavedPath;
  info.LastStatePath = convo.LastStatePath;
  info.Dirty = convo.Dirty;
  info.Closed = convo.Closed;
  return info;
}

std::vector<ConversationInfo> ConvoManager::ListConversations(ModelId model_id) const {
  const auto& model = RequireModel(m_impl->Models, model_id);
  std::vector<ConversationInfo> out;
  out.reserve(model.Convos.size());
  for (const auto& [cid, convo] : model.Convos) {
    (void)cid;
    ConversationInfo info;
    info.Model = model_id;
    info.Id = convo.Id;
    info.Title = convo.Convo ? convo.Convo->GetTitle() : std::nullopt;
    info.LastSavedPath = convo.LastSavedPath;
    info.LastStatePath = convo.LastStatePath;
    info.Dirty = convo.Dirty;
    info.Closed = convo.Closed;
    out.push_back(std::move(info));
  }
  return out;
}

std::string ConvoManager::Chat(ModelId model_id, const std::string& user_message,
                               float temperature, int max_tokens) {
  auto& model = RequireModel(m_impl->Models, model_id);
  if (!model.Active.has_value()) {
    throw std::runtime_error("ConvoManager::Chat: no active conversation for this model");
  }
  auto& convo = RequireConvo(model, model.Active.value());
  if (convo.Closed) {
    throw std::runtime_error("ConvoManager::Chat: active conversation is closed");
  }

  const std::string reply = convo.Convo->Chat(user_message, temperature, max_tokens);
  convo.Dirty = true;
  return reply;
}



std::string ConvoManager::Chat(ModelId model_id, ConvoId convo_id,
                               const std::string& user_message,
                               float temperature, int max_tokens) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (convo.Closed)
    throw std::runtime_error("ConvoManager::Chat: conversation is closed");
  const std::string reply = convo.Convo->Chat(user_message, temperature, max_tokens);
  convo.Dirty = true;
  return reply;
}

void ConvoManager::SetAgentConfig(ModelId model_id, ConvoId convo_id,
                                  int recent_turns_window, bool clear_kv_each_turn) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (!convo.Convo) return;
  convo.Convo->SetRecentTurnsWindow(recent_turns_window);
  convo.Convo->SetClearKvCacheEachTurn(clear_kv_each_turn);
}

void ConvoManager::LoadConvoHistory(ModelId model_id, ConvoId convo_id,
                                    const std::string& path) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (!convo.Convo) throw std::runtime_error("ConvoManager::LoadConvoHistory: null conversation");
  convo.Convo->LoadHistory(path);
  convo.LastSavedPath = path;
}

void ConvoManager::LoadConvoState(ModelId model_id, ConvoId convo_id,
                                  const std::string& path) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (!convo.Convo) throw std::runtime_error("ConvoManager::LoadConvoState: null conversation");
  convo.Convo->LoadState(path);
  convo.LastStatePath = path;
}

void ConvoManager::SaveAllNoThrow() noexcept {
  for (auto& [mid, model] : m_impl->Models) {
    for (auto& [cid, convo] : model.Convos) {
      if (!convo.Dirty || convo.Closed || !convo.Convo) continue;
      try {
        const std::string written = convo.Convo->SaveHistory(
            convo.LastSavedPath.value_or(""));
        const std::string sp = SidecarStatePathForHistoryPath(written);
        convo.Convo->SaveState(sp);
        convo.LastSavedPath = written;
        convo.LastStatePath = sp;
        convo.Dirty = false;
      } catch (...) {}
    }
  }
}

void ConvoManager::Close(ModelId model_id, ConvoId convo_id) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (convo.Dirty && convo.Convo) {
    try {
      const std::string written = convo.Convo->SaveHistory(
          convo.LastSavedPath.value_or(""));
      const std::string sp = SidecarStatePathForHistoryPath(written);
      convo.Convo->SaveState(sp);
      convo.LastSavedPath = written;
      convo.LastStatePath = sp;
      convo.Dirty = false;
    } catch (...) {}
  }
  convo.Closed = true;
  if (model.Active == convo_id) model.Active = std::nullopt;
}

std::string ConvoManager::Save(ModelId model_id, ConvoId convo_id, const std::string& path) {
  auto& model = RequireModel(m_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (!convo.Convo) {
    throw std::runtime_error("ConvoManager::Save: null conversation");
  }

  const std::string written = convo.Convo->SaveHistory(path);
  const std::string state_path = SidecarStatePathForHistoryPath(written);
  convo.Convo->SaveState(state_path);
  convo.LastSavedPath = written;
  convo.LastStatePath = state_path;
  convo.Dirty = false;
  return written;
}

void ConvoManager::SaveSession(const std::string& dir) {
  if (dir.empty()) {
    throw std::invalid_argument("ConvoManager::SaveSession: dir must not be empty");
  }

  const fs::path root(dir);
  const fs::path convos_dir = root / "convos";
  fs::create_directories(convos_dir);

  json manifest;
  manifest["version"] = 1;
  manifest["models"] = json::array();

  for (auto& [mid, model] : m_impl->Models) {
    json jm;
    jm["id"] = mid;
    jm["model_path"] = model.ModelPath;
    jm["context_size"] = model.ContextSize;
    jm["thread_count"] = model.ThreadCount;
    jm["active_conversation"] = model.Active.has_value() ? json(model.Active.value()) : json(nullptr);
    jm["conversations"] = json::array();

    const fs::path model_dir = convos_dir / ("model_" + std::to_string(mid));
    fs::create_directories(model_dir);

    for (auto& [cid, convo] : model.Convos) {
      (void)cid;
      if (!convo.Convo) continue;

      // Save history JSON and state binary under the session directory.
      const std::string base = "convo_" + std::to_string(convo.Id);
      const fs::path history_path = model_dir / (base + ".json");
      const fs::path state_path = model_dir / (base + ".state.bin");

      const std::string written_history = convo.Convo->SaveHistory(history_path.string());
      convo.Convo->SaveState(state_path.string());

      convo.LastSavedPath = written_history;
      convo.LastStatePath = state_path.string();
      convo.Dirty = false;

      json jc;
      jc["id"] = convo.Id;
      jc["history_path"] = fs::relative(history_path, root).string();
      jc["state_path"] = fs::relative(state_path, root).string();
      jc["closed"] = convo.Closed;
      jm["conversations"].push_back(std::move(jc));
    }

    manifest["models"].push_back(std::move(jm));
  }

  const fs::path manifest_path = root / "manifest.json";
  std::ofstream out(manifest_path);
  if (!out.is_open()) {
    throw std::runtime_error("ConvoManager::SaveSession: cannot open \"" + manifest_path.string() + "\" for writing");
  }
  out << manifest.dump(2);
  if (!out) {
    throw std::runtime_error("ConvoManager::SaveSession: write failed for \"" + manifest_path.string() + "\"");
  }
}

} // namespace convo_manager

