#include "ConvoManager.hpp"

#include "convo.hpp"

#include <map>       // std::map
#include <memory>    // std::make_unique
#include <stdexcept>  // std::invalid_argument, std::runtime_error
#include <utility>    // std::move

namespace convo_manager {
namespace {

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

ModelEntry& RequireModel(std::map<ModelId, ModelEntry>& models, ModelId model_id) {
  auto it = models.find(model_id);
  if (it == models.end()) {
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
  : _impl(std::make_unique<Impl>()) {}

ConvoManager::~ConvoManager() noexcept {
  SaveAllNoThrow();
}

ModelId ConvoManager::AddModel(const std::string& model_path, int context_size, int thread_count) {
  if (model_path.empty()) {
    throw std::invalid_argument("ConvoManager::AddModel: model_path must not be empty");
  }

  ModelEntry entry;
  entry.Id = _impl->NextModelId++;
  entry.ModelPath = model_path;
  entry.Model = std::make_unique<AIModel>(model_path, context_size, thread_count);
  _impl->Models.emplace(entry.Id, std::move(entry));
  return entry.Id;
}

std::vector<ModelInfo> ConvoManager::ListModels() const {
  std::vector<ModelInfo> out;
  out.reserve(_impl->Models.size());

  for (const auto& [id, model] : _impl->Models) {
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
  auto& model = RequireModel(_impl->Models, model_id);

  ConvoEntry e;
  e.Id = model.NextConvoId++;
  e.Convo = std::make_unique<AIConvo>(*model.Model, system_prompt);
  if (initial_title && !initial_title->empty()) {
    e.Convo->SetTitle(*initial_title);
  }
  model.Convos.emplace(e.Id, std::move(e));
  model.Active = model.Active.value_or(e.Id);
  return e.Id;
}

void ConvoManager::SwitchActiveConversation(ModelId model_id, ConvoId convo_id) {
  auto& model = RequireModel(_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (convo.Closed) {
    throw std::invalid_argument("ConvoManager::SwitchActiveConversation: conversation is closed");
  }
  model.Active = convo_id;
}

std::optional<std::pair<ModelId, ConvoId>> ConvoManager::GetActive() const {
  for (const auto& [mid, model] : _impl->Models) {
    if (model.Active.has_value()) {
      return std::make_pair(mid, *model.Active);
    }
  }
  return std::nullopt;
}

ConversationInfo ConvoManager::GetConversationInfo(ModelId model_id, ConvoId convo_id) const {
  const auto& model = RequireModel(_impl->Models, model_id);
  const auto& convo = RequireConvo(model, convo_id);
  ConversationInfo info;
  info.Model = model_id;
  info.Id = convo.Id;
  info.Title = convo.Convo ? convo.Convo->GetTitle() : std::nullopt;
  info.LastSavedPath = convo.LastSavedPath;
  info.Dirty = convo.Dirty;
  info.Closed = convo.Closed;
  return info;
}

std::vector<ConversationInfo> ConvoManager::ListConversations(ModelId model_id) const {
  const auto& model = RequireModel(_impl->Models, model_id);
  std::vector<ConversationInfo> out;
  out.reserve(model.Convos.size());
  for (const auto& [cid, convo] : model.Convos) {
    (void)cid;
    ConversationInfo info;
    info.Model = model_id;
    info.Id = convo.Id;
    info.Title = convo.Convo ? convo.Convo->GetTitle() : std::nullopt;
    info.LastSavedPath = convo.LastSavedPath;
    info.Dirty = convo.Dirty;
    info.Closed = convo.Closed;
    out.push_back(std::move(info));
  }
  return out;
}

std::string ConvoManager::Chat(ModelId model_id, const std::string& user_message,
                               float temperature, int max_tokens) {
  auto& model = RequireModel(_impl->Models, model_id);
  if (!model.Active.has_value()) {
    throw std::runtime_error("ConvoManager::Chat: no active conversation for this model");
  }
  auto& convo = RequireConvo(model, *model.Active);
  if (convo.Closed) {
    throw std::runtime_error("ConvoManager::Chat: active conversation is closed");
  }

  const std::string reply = convo.Convo->Chat(user_message, temperature, max_tokens);
  convo.Dirty = true;
  return reply;
}

std::string ConvoManager::Save(ModelId model_id, ConvoId convo_id, const std::string& path) {
  auto& model = RequireModel(_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (!convo.Convo) {
    throw std::runtime_error("ConvoManager::Save: null conversation");
  }

  const std::string written = convo.Convo->SaveHistory(path);
  convo.LastSavedPath = written;
  convo.Dirty = false;
  return written;
}

void ConvoManager::SaveAllNoThrow() noexcept {
  try {
    for (auto& [mid, model] : _impl->Models) {
      for (auto& [cid, convo] : model.Convos) {
        (void)cid;
        if (!convo.Convo) continue;
        if (convo.Closed) continue;
        if (!convo.Dirty && convo.LastSavedPath.has_value()) continue;

        try {
          const std::string written = convo.Convo->SaveHistory("");
          convo.LastSavedPath = written;
          convo.Dirty = false;
        } catch (...) {
          // best-effort only
        }
      }
      (void)mid;
    }
  } catch (...) {
    // best-effort only
  }
}

void ConvoManager::Close(ModelId model_id, ConvoId convo_id) {
  auto& model = RequireModel(_impl->Models, model_id);
  auto& convo = RequireConvo(model, convo_id);
  if (convo.Closed) return;

  try {
    if (convo.Dirty || !convo.LastSavedPath.has_value()) {
      const std::string written = convo.Convo->SaveHistory("");
      convo.LastSavedPath = written;
      convo.Dirty = false;
    }
  } catch (...) {
    // best-effort save
  }

  convo.Closed = true;
}

} // namespace convo_manager

