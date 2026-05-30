// prompt_loader.cpp — PromptLoader implementation
//
// Thread-safety: std::shared_mutex guards the cache and prompts_dir.
//   - load/render (reads) use std::shared_lock for the cache check.
//     On a cache miss the lock is temporarily released to do disk I/O,
//     then re-acquired as a unique_lock to insert.  Two racing misses are
//     harmless: the second write is idempotent.
//   - reload/setPromptsDir use std::unique_lock (rare, write path).
#include "agent/prompt_loader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace agent {

PromptLoader::PromptLoader(std::filesystem::path prompts_dir)
    : m_prompts_dir(std::move(prompts_dir))
{}

std::string PromptLoader::load(const std::string& name)
{
    // Fast path: shared read of cache
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_cache.find(name);
        if (it != m_cache.end()) return it->second;
    }

    // Cache miss — read file outside the lock (disk I/O)
    std::filesystem::path fpath;
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        fpath = m_prompts_dir / (name + ".md");
    }

    std::ifstream ifs(fpath);
    if (!ifs.is_open())
        throw std::runtime_error("PromptLoader: file not found: " + fpath.string());

    std::ostringstream buf;
    buf << ifs.rdbuf();
    std::string content = buf.str();

    // Insert into cache (idempotent if another thread beat us)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_cache.emplace(name, content);
    }
    return content;
}

void PromptLoader::reload()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_cache.clear();
}

std::string PromptLoader::substitute(
    const std::string&                        template_str,
    const std::map<std::string, std::string>& vars)
{
    // Pure function — no lock needed.
    std::string result;
    result.reserve(template_str.size() * 2);

    std::size_t pos = 0;
    while (pos < template_str.size()) {
        std::size_t open = template_str.find("{{", pos);
        if (open == std::string::npos) {
            result.append(template_str, pos, std::string::npos);
            break;
        }
        result.append(template_str, pos, open - pos);

        std::size_t close = template_str.find("}}", open + 2);
        if (close == std::string::npos) {
            result.append(template_str, open, std::string::npos);
            break;
        }

        std::string key = template_str.substr(open + 2, close - open - 2);
        auto it = vars.find(key);
        if (it == vars.end()) {
            throw std::runtime_error(
                "PromptLoader: no value for placeholder '{{" + key + "}}'");
        }
        result.append(it->second);
        pos = close + 2;
    }
    return result;
}

std::string PromptLoader::render(const std::string&                        name,
                                  const std::map<std::string, std::string>& vars)
{
    return substitute(load(name), vars);
}

void PromptLoader::setPromptsDir(std::filesystem::path dir)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_prompts_dir = std::move(dir);
    m_cache.clear();
}

} // namespace agent
