// test_prompt_loader.cpp — PromptLoader unit tests
//
// Covers:
//   - load() reads a .md file and caches it
//   - reload() clears the cache and re-reads from disk
//   - substitute() replaces all {{KEY}} placeholders
//   - substitute() throws when a placeholder has no matching key
//   - render() = load() + substitute() combined
//   - Missing file throws std::runtime_error

#include "test_helper.hpp"
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

static const std::string PROMPTS_DIR = "/tmp/test_prompt_loader_dir";

// Write a file to the temp prompts directory
static void writeFile(const std::string& name, const std::string& content) {
    fs::create_directories(PROMPTS_DIR);
    std::ofstream f(PROMPTS_DIR + "/" + name + ".md");
    f << content;
}

int main() {
    std::cout << "=== test_prompt_loader ===\n";

    // Reset state
    fs::remove_all(PROMPTS_DIR);
    fs::create_directories(PROMPTS_DIR);

    // ── Section 1: load() reads and caches ───────────────────────────────────
    test::section("load() reads file and caches");
    {
        writeFile("greet", "Hello, {{NAME}}! You are {{AGE}} years old.");

        agent::PromptLoader loader(PROMPTS_DIR);
        std::string content;
        CHECK_NOTHROW(content = loader.load("greet"));
        CHECK_EQ(content, std::string("Hello, {{NAME}}! You are {{AGE}} years old."));

        // Second load should return the cached value (no disk read)
        // Overwrite the file to confirm caching
        writeFile("greet", "OVERWRITTEN");
        std::string cached;
        CHECK_NOTHROW(cached = loader.load("greet"));
        // Should still be the original (cached) content
        CHECK_EQ(cached, std::string("Hello, {{NAME}}! You are {{AGE}} years old."));
    }

    // ── Section 2: reload() clears cache ─────────────────────────────────────
    test::section("reload() clears cache");
    {
        writeFile("versioned", "version 1");

        agent::PromptLoader loader(PROMPTS_DIR);
        std::string v1 = loader.load("versioned");
        CHECK_EQ(v1, std::string("version 1"));

        // Overwrite the file then reload
        writeFile("versioned", "version 2");
        loader.reload();

        std::string v2 = loader.load("versioned");
        CHECK_EQ(v2, std::string("version 2"));
    }

    // ── Section 3: substitute() replaces all {{KEY}} ─────────────────────────
    test::section("substitute() replaces all placeholders");
    {
        agent::PromptLoader loader(PROMPTS_DIR);
        std::string tmpl = "Dear {{TITLE}} {{LAST_NAME}},\nYour score is {{SCORE}}.";
        std::map<std::string, std::string> vars = {
            {"TITLE",     "Dr."},
            {"LAST_NAME", "Smith"},
            {"SCORE",     "99"}
        };

        std::string result;
        CHECK_NOTHROW(result = loader.substitute(tmpl, vars));
        CHECK_EQ(result, std::string("Dear Dr. Smith,\nYour score is 99."));
    }

    // ── Section 4: substitute() with extra keys (silently ignored) ────────────
    test::section("substitute() ignores extra vars");
    {
        agent::PromptLoader loader(PROMPTS_DIR);
        std::string tmpl = "Value: {{X}}";
        std::map<std::string, std::string> vars = {
            {"X",   "42"},
            {"Y",   "unused"},   // extra key — must not cause an error
            {"ZZZ", "ignored"}
        };
        std::string result;
        CHECK_NOTHROW(result = loader.substitute(tmpl, vars));
        CHECK_EQ(result, std::string("Value: 42"));
    }

    // ── Section 5: substitute() throws on missing placeholder value ───────────
    test::section("substitute() throws on unresolved placeholder");
    {
        agent::PromptLoader loader(PROMPTS_DIR);
        std::string tmpl = "Hello {{NAME}}, your code is {{CODE}}.";
        std::map<std::string, std::string> vars = {
            {"NAME", "Alice"}
            // {{CODE}} is deliberately absent
        };
        CHECK_THROW(loader.substitute(tmpl, vars));
    }

    // ── Section 6: render() = load() + substitute() ───────────────────────────
    test::section("render() combines load and substitute");
    {
        writeFile("task_template", "TASK: {{TASK}}\nCATALOG: {{CATALOG}}");

        agent::PromptLoader loader(PROMPTS_DIR);
        std::string rendered;
        CHECK_NOTHROW(rendered = loader.render("task_template", {
            {"TASK",    "research AI Act"},
            {"CATALOG", "[Action1, Action2]"}
        }));
        CHECK_EQ(rendered, std::string("TASK: research AI Act\nCATALOG: [Action1, Action2]"));
    }

    // ── Section 7: Missing file throws ───────────────────────────────────────
    test::section("load() throws for missing file");
    {
        agent::PromptLoader loader(PROMPTS_DIR);
        CHECK_THROW(loader.load("file_that_does_not_exist_xyz"));
    }

    // ── Section 8: setPromptsDir + reload switches directory ─────────────────
    test::section("setPromptsDir switches directory");
    {
        const std::string dir2 = "/tmp/test_prompt_loader_dir2";
        fs::create_directories(dir2);
        {
            std::ofstream f(dir2 + "/alt.md");
            f << "FROM_DIR2";
        }

        agent::PromptLoader loader(PROMPTS_DIR);
        loader.setPromptsDir(dir2);

        std::string content;
        CHECK_NOTHROW(content = loader.load("alt"));
        CHECK_EQ(content, std::string("FROM_DIR2"));

        fs::remove_all(dir2);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
