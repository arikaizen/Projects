#pragma once
#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace agent {

// Loads system-prompt templates from disk, caches them, and performs
// {{PLACEHOLDER}} substitution.  Every Stage uses this component to build its
// per-call prompt.
class PromptLoader {
public:
    explicit PromptLoader(std::filesystem::path prompts_dir);

    // Load a template by name (e.g. "reason_stage" -> "<dir>/reason_stage.md").
    // Cached after first load.  Throws std::runtime_error if the file is absent.
    std::string load(const std::string& name);

    // Drop cache so the next load() re-reads from disk.
    void reload();

    // Replace every {{KEY}} with vars[KEY].
    // Throws if the template contains a placeholder with no matching key in vars.
    // Extra keys in vars are silently ignored.
    std::string substitute(const std::string& template_str,
                           const std::map<std::string, std::string>& vars);

    // Convenience: load(name) + substitute.
    std::string render(const std::string& name,
                       const std::map<std::string, std::string>& vars);

    void setPromptsDir(std::filesystem::path dir);
    std::filesystem::path promptsDir() const { return m_prompts_dir; }

private:
    mutable std::shared_mutex  m_mutex;  // shared_lock for reads, unique_lock for reload/setDir
    std::filesystem::path      m_prompts_dir;
    std::map<std::string, std::string> m_cache;
};

} // namespace agent
