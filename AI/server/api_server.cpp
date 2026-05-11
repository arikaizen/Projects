// api_server.cpp
// REST API server wrapping ConvoManager.
//
// Endpoints:
//   GET  /api/health
//   GET  /api/models
//   POST /api/models                       { model_path, context_size? }
//   GET  /api/conversations
//   POST /api/conversations                { model_id, system_prompt?, title? }
//   GET  /api/conversations/:id
//   GET  /api/conversations/:id/history
//   POST /api/conversations/:id/chat       { message, temperature?, max_tokens? }
//   DELETE /api/conversations/:id
//   GET  /api/agents
//   POST /api/agents                       { name, description, system_prompt, model_id }
//   DELETE /api/agents/:id
//   POST /api/agents/:id/chat              { message, temperature?, max_tokens? }

#include "api_server.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT 0
#include "httplib.h"

#include "ConvoManager.hpp"
#include "convo.hpp"

#include <atomic>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace ai_api {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Agent store — persisted as a JSON file
// ─────────────────────────────────────────────────────────────────────────────

struct Agent {
    uint32_t    id          = 0;
    std::string name;
    std::string description;
    std::string system_prompt;
    uint32_t    model_id    = 0;
    // Each agent maintains its own conversation per model session.
    // convo_id == 0 means not yet created.
    uint32_t    convo_id    = 0;
};

struct AgentStore {
    std::map<uint32_t, Agent> agents;
    uint32_t next_id = 1;
    std::string persist_path;
    std::mutex  mu;

    void Load() {
        if (persist_path.empty() || !fs::exists(persist_path)) return;
        std::ifstream f(persist_path);
        if (!f.is_open()) return;
        try {
            json j; f >> j;
            next_id = j.value("next_id", 1u);
            for (auto& ja : j.value("agents", json::array())) {
                Agent a;
                a.id           = ja.at("id").get<uint32_t>();
                a.name         = ja.value("name", "");
                a.description  = ja.value("description", "");
                a.system_prompt= ja.at("system_prompt").get<std::string>();
                a.model_id     = ja.at("model_id").get<uint32_t>();
                a.convo_id     = 0; // conversations are runtime-only
                agents[a.id]   = a;
            }
        } catch (...) {}
    }

    void Save() {
        if (persist_path.empty()) return;
        json j;
        j["next_id"] = next_id;
        j["agents"]  = json::array();
        for (auto& [id, a] : agents) {
            json ja;
            ja["id"]           = a.id;
            ja["name"]         = a.name;
            ja["description"]  = a.description;
            ja["system_prompt"]= a.system_prompt;
            ja["model_id"]     = a.model_id;
            j["agents"].push_back(ja);
        }
        std::ofstream f(persist_path);
        if (f.is_open()) f << j.dump(2);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static json ConvoInfoToJson(const convo_manager::ConversationInfo& c) {
    json j;
    j["id"]      = c.Id;
    j["model_id"]= c.Model;
    j["dirty"]   = c.Dirty;
    j["closed"]  = c.Closed;
    j["title"]   = c.Title.value_or("");
    j["saved_path"] = c.LastSavedPath.value_or("");
    return j;
}

static json ModelInfoToJson(const convo_manager::ModelInfo& m) {
    json j;
    j["id"]   = m.Id;
    j["path"] = m.ModelPath;
    j["active_conversation"] = m.ActiveConversation.has_value()
                                 ? json(m.ActiveConversation.value())
                                 : json(nullptr);
    j["conversations"] = json::array();
    for (auto& c : m.Conversations)
        j["conversations"].push_back(ConvoInfoToJson(c));
    return j;
}

static void SetCors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

static void Ok(httplib::Response& res, const json& body) {
    SetCors(res);
    res.set_content(body.dump(), "application/json");
}

static void Err(httplib::Response& res, int code, const std::string& msg) {
    SetCors(res);
    res.status = code;
    res.set_content(json{{"error", msg}}.dump(), "application/json");
}

// Flat map of convo_id -> model_id so we can look up by convo id alone.
struct ConvoRegistry {
    std::map<uint32_t, uint32_t> convo_to_model; // convo_id -> model_id
    std::mutex mu;

    void Add(uint32_t convo_id, uint32_t model_id) {
        std::lock_guard<std::mutex> lk(mu);
        convo_to_model[convo_id] = model_id;
    }
    std::optional<uint32_t> ModelFor(uint32_t convo_id) {
        std::lock_guard<std::mutex> lk(mu);
        auto it = convo_to_model.find(convo_id);
        if (it == convo_to_model.end()) return std::nullopt;
        return it->second;
    }
    void Remove(uint32_t convo_id) {
        std::lock_guard<std::mutex> lk(mu);
        convo_to_model.erase(convo_id);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// RunServer
// ─────────────────────────────────────────────────────────────────────────────

void RunServer(const std::string& host,
               int                port,
               const std::vector<std::string>& model_paths,
               int  context_size,
               const std::string& agents_file)
{
    convo_manager::ConvoManager manager;
    std::mutex manager_mu;
    ConvoRegistry registry;
    AgentStore agent_store;
    agent_store.persist_path = agents_file;
    agent_store.Load();

    // Pre-load models passed on command line.
    for (const auto& mp : model_paths) {
        std::lock_guard<std::mutex> lk(manager_mu);
        const auto mid = manager.AddModel(mp, context_size);
        (void)mid;
    }

    httplib::Server svr;

    // ── CORS preflight ────────────────────────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        SetCors(res);
        res.status = 204;
    });

    // ── Health ────────────────────────────────────────────────────────────────
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        Ok(res, {{"status", "ok"}});
    });

    // ── Models ────────────────────────────────────────────────────────────────
    svr.Get("/api/models", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(manager_mu);
        json arr = json::array();
        for (auto& m : manager.ListModels())
            arr.push_back(ModelInfoToJson(m));
        Ok(res, arr);
    });

    svr.Post("/api/models", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) {
            return Err(res, 400, "invalid JSON");
        }
        if (!body.contains("model_path"))
            return Err(res, 400, "model_path required");

        const std::string mp = body["model_path"].get<std::string>();
        const int ctx = body.value("context_size", context_size);
        try {
            uint32_t mid;
            {
                std::lock_guard<std::mutex> lk(manager_mu);
                mid = manager.AddModel(mp, ctx);
            }
            Ok(res, {{"id", mid}, {"model_path", mp}});
        } catch (const std::exception& e) {
            Err(res, 500, e.what());
        }
    });

    // ── Conversations ─────────────────────────────────────────────────────────
    svr.Get("/api/conversations", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(manager_mu);
        json arr = json::array();
        for (auto& m : manager.ListModels())
            for (auto& c : m.Conversations)
                arr.push_back(ConvoInfoToJson(c));
        Ok(res, arr);
    });

    svr.Post("/api/conversations", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) {
            return Err(res, 400, "invalid JSON");
        }
        if (!body.contains("model_id"))
            return Err(res, 400, "model_id required");

        const uint32_t mid  = body["model_id"].get<uint32_t>();
        const std::string sp= body.value("system_prompt", "You are a helpful assistant.");
        const std::string tt= body.value("title", "");
        try {
            uint32_t cid;
            {
                std::lock_guard<std::mutex> lk(manager_mu);
                cid = manager.NewConversation(mid, sp, tt.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{tt});
                manager.SwitchActiveConversation(mid, cid);
            }
            registry.Add(cid, mid);
            auto info = manager.GetConversationInfo(mid, cid);
            Ok(res, ConvoInfoToJson(info));
        } catch (const std::exception& e) {
            Err(res, 500, e.what());
        }
    });

    svr.Get(R"(/api/conversations/(\d+))",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t cid = std::stoul(req.matches[1]);
        auto mid_opt = registry.ModelFor(cid);
        if (!mid_opt) return Err(res, 404, "conversation not found");
        try {
            std::lock_guard<std::mutex> lk(manager_mu);
            auto info = manager.GetConversationInfo(*mid_opt, cid);
            Ok(res, ConvoInfoToJson(info));
        } catch (const std::exception& e) {
            Err(res, 500, e.what());
        }
    });

    svr.Get(R"(/api/conversations/(\d+)/history)",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t cid = std::stoul(req.matches[1]);
        auto mid_opt = registry.ModelFor(cid);
        if (!mid_opt) return Err(res, 404, "conversation not found");
        // We don't expose raw history via ConvoManager yet, so return stub.
        Ok(res, json::array());
    });

    svr.Post(R"(/api/conversations/(\d+)/chat)",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t cid = std::stoul(req.matches[1]);
        auto mid_opt = registry.ModelFor(cid);
        if (!mid_opt) return Err(res, 404, "conversation not found");

        json body;
        try { body = json::parse(req.body); } catch (...) {
            return Err(res, 400, "invalid JSON");
        }
        if (!body.contains("message"))
            return Err(res, 400, "message required");

        const std::string msg  = body["message"].get<std::string>();
        const float temp       = body.value("temperature", 0.7f);
        const int   max_tokens = body.value("max_tokens", 2048);

        try {
            std::lock_guard<std::mutex> lk(manager_mu);
            manager.SwitchActiveConversation(*mid_opt, cid);
            const std::string reply = manager.Chat(*mid_opt, msg, temp, max_tokens);
            Ok(res, {{"reply", reply}, {"convo_id", cid}});
        } catch (const std::exception& e) {
            Err(res, 500, e.what());
        }
    });

    svr.Delete(R"(/api/conversations/(\d+))",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t cid = std::stoul(req.matches[1]);
        registry.Remove(cid);
        SetCors(res);
        res.status = 204;
    });

    // ── Agents ────────────────────────────────────────────────────────────────
    svr.Get("/api/agents", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(agent_store.mu);
        json arr = json::array();
        for (auto& [id, a] : agent_store.agents) {
            json ja;
            ja["id"]           = a.id;
            ja["name"]         = a.name;
            ja["description"]  = a.description;
            ja["system_prompt"]= a.system_prompt;
            ja["model_id"]     = a.model_id;
            arr.push_back(ja);
        }
        Ok(res, arr);
    });

    svr.Post("/api/agents", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try { body = json::parse(req.body); } catch (...) {
            return Err(res, 400, "invalid JSON");
        }
        if (!body.contains("name") || !body.contains("system_prompt") || !body.contains("model_id"))
            return Err(res, 400, "name, system_prompt, and model_id required");

        Agent a;
        {
            std::lock_guard<std::mutex> lk(agent_store.mu);
            a.id           = agent_store.next_id++;
            a.name         = body["name"].get<std::string>();
            a.description  = body.value("description", "");
            a.system_prompt= body["system_prompt"].get<std::string>();
            a.model_id     = body["model_id"].get<uint32_t>();
            agent_store.agents[a.id] = a;
            agent_store.Save();
        }

        json ja;
        ja["id"]           = a.id;
        ja["name"]         = a.name;
        ja["description"]  = a.description;
        ja["system_prompt"]= a.system_prompt;
        ja["model_id"]     = a.model_id;
        Ok(res, ja);
    });

    svr.Delete(R"(/api/agents/(\d+))",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t aid = std::stoul(req.matches[1]);
        {
            std::lock_guard<std::mutex> lk(agent_store.mu);
            agent_store.agents.erase(aid);
            agent_store.Save();
        }
        SetCors(res);
        res.status = 204;
    });

    svr.Post(R"(/api/agents/(\d+)/chat)",
        [&](const httplib::Request& req, httplib::Response& res) {
        const uint32_t aid = std::stoul(req.matches[1]);

        Agent* agent_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lk(agent_store.mu);
            auto it = agent_store.agents.find(aid);
            if (it == agent_store.agents.end())
                return Err(res, 404, "agent not found");
            agent_ptr = &it->second;
        }

        json body;
        try { body = json::parse(req.body); } catch (...) {
            return Err(res, 400, "invalid JSON");
        }
        if (!body.contains("message"))
            return Err(res, 400, "message required");

        const std::string msg  = body["message"].get<std::string>();
        const float temp       = body.value("temperature", 0.7f);
        const int   max_tokens = body.value("max_tokens", 2048);

        // Create a conversation for this agent if it doesn't have one yet.
        uint32_t mid  = agent_ptr->model_id;
        uint32_t cid  = agent_ptr->convo_id;
        try {
            std::lock_guard<std::mutex> lk(manager_mu);
            if (cid == 0) {
                cid = manager.NewConversation(mid, agent_ptr->system_prompt,
                      std::optional<std::string>{"Agent: " + agent_ptr->name});
                manager.SwitchActiveConversation(mid, cid);
                {
                    std::lock_guard<std::mutex> alk(agent_store.mu);
                    agent_store.agents[aid].convo_id = cid;
                }
                registry.Add(cid, mid);
            } else {
                manager.SwitchActiveConversation(mid, cid);
            }
            const std::string reply = manager.Chat(mid, msg, temp, max_tokens);
            Ok(res, {{"reply", reply}, {"agent_id", aid}, {"convo_id", cid}});
        } catch (const std::exception& e) {
            Err(res, 500, e.what());
        }
    });

    // ── Static files (serve the web UI) ──────────────────────────────────────
    svr.set_mount_point("/", "./webui/dist");

    std::cout << "AI API server listening on " << host << ":" << port << "\n";
    std::cout << "Web UI: http://" << host << ":" << port << "/\n";

    svr.listen(host, port);
}

} // namespace ai_api
