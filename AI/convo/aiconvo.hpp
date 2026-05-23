#pragma once
#include "aimodel.hpp"
#include "types.hpp"
#include <optional>
#include <string>
#include <vector>

class AIConvo {
public:
    AIConvo(AIModel& model, const std::string& system_prompt);
    virtual ~AIConvo() = default;
    AIConvo(const AIConvo&)            = delete;
    AIConvo& operator=(const AIConvo&) = delete;
    AIConvo(AIConvo&&)                 = delete;
    AIConvo& operator=(AIConvo&&)      = delete;

    std::string Chat(const std::string& message,
                     float temperature = 0.7f, int max_tokens = 512);

    void                       ClearHistory() noexcept;
    std::vector<Message>       GetHistory() const;
    std::string                SaveHistory(const std::string& path = "");
    void                       LoadHistory(const std::string& path);

    void                       SetSystemPromptFile(const std::string& path);
    std::optional<std::string> GetSystemPromptFile() const noexcept;
    void                       SetRecentTurnsWindow(int turns);
    int                        GetRecentTurnsWindow() const noexcept;
    std::string                GetSummary() const;
    std::optional<std::string> GetTitle() const noexcept;
    void                       SetTitle(const std::string& title);
    AIModel&                   GetModel() const noexcept;

protected:
    virtual std::string GenerateReply(const std::vector<Message>& prompt_messages,
                                      float temperature, int max_tokens) = 0;
    virtual void OnChatFailure() noexcept {}

    std::vector<Message> BuildPromptMessages() const;
    std::string          LoadSystemPromptText() const;

    AIModel&                   m_model;
    std::string                m_system_prompt;
    std::optional<std::string> m_system_prompt_file;
    std::vector<Message>       m_history;
    std::optional<std::string> m_title;
    std::string                m_summary;
    int                        m_recent_turns_window = 10;

private:
    std::vector<Message> GetRecentWindowMessages() const;
    void                 UpdateSummaryNoThrow();
    std::string          MakeTitle(const std::string& first_msg);
};
