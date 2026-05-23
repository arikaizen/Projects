#include "aiconvo.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace {

bool IsBlank(const std::string& s) {
    for (unsigned char c : s) if (!std::isspace(c)) return false;
    return true;
}

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
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AIConvo::AIConvo(AIModel& model, const std::string& system_prompt)
    : m_model(model)
    , m_system_prompt(system_prompt)
{
    if (IsBlank(system_prompt))
        throw std::invalid_argument("AIConvo: system_prompt must not be blank");
    m_history.push_back({Role::System, system_prompt});
}

// ─────────────────────────────────────────────────────────────────────────────
// Public accessors
// ─────────────────────────────────────────────────────────────────────────────

AIModel& AIConvo::GetModel() const noexcept {
    return m_model;
}

std::optional<std::string> AIConvo::GetSystemPromptFile() const noexcept {
    return m_system_prompt_file;
}

void AIConvo::SetSystemPromptFile(const std::string& path) {
    if (IsBlank(path))
        throw std::invalid_argument("AIConvo::SetSystemPromptFile: path must not be blank");
    m_system_prompt_file = path;
}

void AIConvo::SetRecentTurnsWindow(int turns) {
    if (turns < 0)
        throw std::invalid_argument("AIConvo::SetRecentTurnsWindow: turns must be >= 0");
    m_recent_turns_window = turns;
}

int AIConvo::GetRecentTurnsWindow() const noexcept {
    return m_recent_turns_window;
}

std::string AIConvo::GetSummary() const {
    return m_summary;
}

std::optional<std::string> AIConvo::GetTitle() const noexcept {
    return m_title;
}

void AIConvo::SetTitle(const std::string& title) {
    if (IsBlank(title))
        throw std::invalid_argument("AIConvo::SetTitle: title must not be blank");
    m_title = title;
}

// ─────────────────────────────────────────────────────────────────────────────
// Protected helpers
// ─────────────────────────────────────────────────────────────────────────────

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

std::vector<Message> AIConvo::BuildPromptMessages() const {
    std::vector<Message> prompt_messages;
    prompt_messages.reserve(2 + m_history.size());
    prompt_messages.push_back({Role::System, LoadSystemPromptText()});
    if (!IsBlank(m_summary)) {
        prompt_messages.push_back({Role::System, std::string("Conversation summary (hidden state):\n") + m_summary});
    }
    const auto window = GetRecentWindowMessages();
    prompt_messages.insert(prompt_messages.end(), window.begin(), window.end());
    return prompt_messages;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Message> AIConvo::GetRecentWindowMessages() const {
    if (m_recent_turns_window <= 0) return {};

    std::vector<Message> non_system;
    non_system.reserve(m_history.size());
    for (std::size_t i = 0; i < m_history.size(); ++i) {
        if (m_history[i].role == Role::System) continue;
        non_system.push_back(m_history[i]);
    }

    const std::size_t max_msgs = static_cast<std::size_t>(m_recent_turns_window) * 2;
    if (non_system.size() <= max_msgs) return non_system;
    return {non_system.end() - static_cast<std::ptrdiff_t>(max_msgs), non_system.end()};
}

void AIConvo::UpdateSummaryNoThrow() {
    try {
        if (m_history.size() < 2) return;

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

        const std::string new_summary = m_model.Generate(prompt, 0.2f, 256);
        if (!IsBlank(new_summary)) m_summary = new_summary;
    } catch (...) {
        // Summary is best-effort.
    }
}

std::string AIConvo::MakeTitle(const std::string& first_msg) {
    const std::string title_prompt =
        "In 5 words or fewer, give a short title for this conversation.\n"
        "Message: " + first_msg + "\nTitle:";
    try {
        std::string raw_title = m_model.Generate(title_prompt, 0.3f, 20);
        auto title_start = raw_title.find_first_not_of(" \t\n\r");
        auto title_end   = raw_title.find_last_not_of(" \t\n\r");
        if (title_start == std::string::npos)
            return first_msg.substr(0, 40);
        return raw_title.substr(title_start, title_end - title_start + 1);
    } catch (...) {
        return first_msg.substr(0, std::min<std::size_t>(40, first_msg.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

std::string AIConvo::Chat(const std::string& message, float temperature, int max_tokens) {
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

    bool is_first_user_message = true;
    for (const auto& history_entry : m_history)
        if (history_entry.role == Role::User) { is_first_user_message = false; break; }

    m_history.push_back({Role::User, message});

    try {
        auto prompt_messages = BuildPromptMessages();
        std::string reply = GenerateReply(prompt_messages, temperature, max_tokens);

        if (IsBlank(reply))
            throw std::runtime_error("AIConvo::Chat: model returned an empty response");

        m_history.push_back({Role::Assistant, reply});

        UpdateSummaryNoThrow();

        if (is_first_user_message && !m_title.has_value()) {
            try { m_title = MakeTitle(message); }
            catch (...) {}
        }

        return reply;

    } catch (...) {
        m_history.pop_back();
        OnChatFailure();
        throw;
    }
}

void AIConvo::ClearHistory() noexcept {
    if (m_history.size() > 1)
        m_history.erase(m_history.begin() + 1, m_history.end());
}

std::vector<Message> AIConvo::GetHistory() const {
    return m_history;
}

std::string AIConvo::SaveHistory(const std::string& path) {
    json json_document;
    json_document["title"] = m_title.has_value() ? json(*m_title) : json(nullptr);
    json_document["system_prompt_file"] = m_system_prompt_file.has_value() ? json(*m_system_prompt_file) : json(nullptr);
    json_document["recent_turns_window"] = m_recent_turns_window;
    json_document["summary"] = IsBlank(m_summary) ? json(nullptr) : json(m_summary);
    json_document["model_name"] = m_model.GetModelName();

    json json_messages = json::array();
    for (const auto& history_entry : m_history)
        json_messages.push_back({
            {"role",    RoleToStr(history_entry.role)},
            {"content", history_entry.content}
        });
    json_document["messages"] = json_messages;

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
            "AIConvo::SaveHistory: cannot open \"" + output_file_path + "\" for writing");

    output_file << json_document.dump(2);
    if (!output_file)
        throw std::runtime_error(
            "AIConvo::SaveHistory: write failed for \"" + output_file_path + "\"");

    return output_file_path;
}

void AIConvo::LoadHistory(const std::string& path) {
    std::ifstream input_file(path);
    if (!input_file.is_open())
        throw std::runtime_error("AIConvo::LoadHistory: cannot open \"" + path + "\"");

    json json_document;
    try {
        input_file >> json_document;
    } catch (const json::parse_error& parse_error) {
        throw std::invalid_argument(
            std::string("AIConvo::LoadHistory: malformed JSON in \"")
            + path + "\": " + parse_error.what());
    }

    if (!json_document.is_object())
        throw std::invalid_argument(
            "AIConvo::LoadHistory: expected a JSON object at top level");
    if (!json_document.contains("messages") || !json_document["messages"].is_array())
        throw std::invalid_argument(
            "AIConvo::LoadHistory: JSON must contain a \"messages\" array");

    std::vector<Message> loaded_history;
    for (const auto& item : json_document["messages"]) {
        if (!item.contains("role") || !item["role"].is_string())
            throw std::invalid_argument(
                "AIConvo::LoadHistory: each message must have a string \"role\"");
        if (!item.contains("content") || !item["content"].is_string())
            throw std::invalid_argument(
                "AIConvo::LoadHistory: each message must have a string \"content\"");

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

    // model_name field is optional (backward compat with old saves).

    m_history = std::move(loaded_history);
    m_title   = std::move(loaded_title);
    m_system_prompt_file = std::move(loaded_system_prompt_file);
    m_recent_turns_window = loaded_recent_turns_window < 0 ? 0 : loaded_recent_turns_window;
    m_summary = std::move(loaded_summary);
}
