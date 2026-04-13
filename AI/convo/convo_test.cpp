/**
 * convo_test.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Test suite for convo.hpp / convo.cpp.
 *
 * Structure
 * ─────────
 *   Part 1  — Pure unit tests (no model required).
 *             Tests Role helpers and Message struct.
 *
 *   Part 2  — Integration tests (require a real GGUF model on disk).
 *             Pass the model path as the first command-line argument to run:
 *               ./convo_test /path/to/model.gguf
 *
 * Build (example — adjust paths for your llama.cpp install):
 *   g++ -std=c++17 -O2 \
 *       -I/path/to/llama.cpp/include \
 *       -I/path/to/nlohmann \
 *       convo.cpp convo_test.cpp \
 *       -L/path/to/llama.cpp/build -lllama \
 *       -lpthread -ldl \
 *       -o convo_test
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "convo.hpp"

#include <cstdio>     // std::remove
#include <cstdlib>    // (C stdlib helpers)
#include <fstream>    // std::ifstream, std::ofstream
#include <iostream>   // std::cout, std::cerr
#include <sstream>    // std::ostringstream
#include <stdexcept>  // std::invalid_argument, std::runtime_error
#include <string>     // std::string

// ─────────────────────────────────────────────────────────────────────────────
// Lightweight test framework
// ─────────────────────────────────────────────────────────────────────────────

namespace test {

static int pass_count  = 0;
static int fail_count  = 0;
static int total_count = 0;

static void record(bool ok, const char* expr, const char* file, int line) {
    ++total_count;
    if (ok) {
        ++pass_count;
        std::cout << "  [PASS] " << expr << "\n";
    } else {
        ++fail_count;
        std::cout << "  [FAIL] " << expr
                  << "  ← " << file << ":" << line << "\n";
    }
}

static void section(const char* name) {
    std::cout << "\n── " << name << " ──\n";
}

static void summary() {
    std::cout << "\n══════════════════════════════════════════\n"
              << "  Results: " << pass_count << " passed, "
              << fail_count  << " failed, "
              << total_count << " total\n"
              << "══════════════════════════════════════════\n";
}

static bool all_passed() { return fail_count == 0; }

} // namespace test

// Assertion macros
#define CHECK(expr)      test::record(!!(expr),     #expr,            __FILE__, __LINE__)
#define CHECK_EQ(a, b)   test::record((a) == (b),   #a " == " #b,    __FILE__, __LINE__)
#define CHECK_NE(a, b)   test::record((a) != (b),   #a " != " #b,    __FILE__, __LINE__)
#define CHECK_GT(a, b)   test::record((a)  > (b),   #a " > "  #b,    __FILE__, __LINE__)
#define CHECK_GE(a, b)   test::record((a) >= (b),   #a " >= " #b,    __FILE__, __LINE__)

/** Verify that EXPR throws an exception of TYPE. */
#define CHECK_THROWS(type, expr)                                               \
    do {                                                                        \
        bool exception_was_thrown = false;                                      \
        try { (void)(expr); }                                                   \
        catch (const type&) { exception_was_thrown = true; }                   \
        catch (...) {}                                                          \
        test::record(exception_was_thrown,                                      \
                     "throws " #type ": " #expr, __FILE__, __LINE__);           \
    } while (0)

/** Verify that EXPR does NOT throw. */
#define CHECK_NOTHROW(expr)                                                    \
    do {                                                                        \
        bool exception_was_thrown = false;                                      \
        try { (void)(expr); }                                                   \
        catch (...) { exception_was_thrown = true; }                            \
        test::record(!exception_was_thrown,                                     \
                     "no-throw: " #expr, __FILE__, __LINE__);                   \
    } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// Helper: write a string to a temporary file; return the path
// ─────────────────────────────────────────────────────────────────────────────

static std::string write_temp_file(const std::string& contents,
                                   const std::string& suffix = ".json") {
    std::string file_path = "/tmp/convo_test_" + suffix;
    std::ofstream temp_file(file_path);
    temp_file << contents;
    return file_path;
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 1 — Pure unit tests (no model needed)
// ─────────────────────────────────────────────────────────────────────────────

static void test_role_helpers() {
    test::section("Role helpers");

    // role_to_str
    CHECK_EQ(RoleToStr(Role::System),    std::string("system"));
    CHECK_EQ(RoleToStr(Role::User),      std::string("user"));
    CHECK_EQ(RoleToStr(Role::Assistant), std::string("assistant"));

    // role_from_str round-trips
    CHECK_EQ(RoleFromStr("system"),    Role::System);
    CHECK_EQ(RoleFromStr("user"),      Role::User);
    CHECK_EQ(RoleFromStr("assistant"), Role::Assistant);

    // Unknown string throws
    CHECK_THROWS(std::invalid_argument, RoleFromStr("robot"));
    CHECK_THROWS(std::invalid_argument, RoleFromStr(""));
    CHECK_THROWS(std::invalid_argument, RoleFromStr("System"));  // case sensitive
}

static void test_message_struct() {
    test::section("Message struct");

    Message original_message{Role::User, "hello"};
    CHECK_EQ(original_message.role,    Role::User);
    CHECK_EQ(original_message.content, std::string("hello"));

    // Copy
    Message copied_message = original_message;
    CHECK_EQ(copied_message.role,    Role::User);
    CHECK_EQ(copied_message.content, std::string("hello"));

    // Mutate original does not affect copy
    original_message.content = "world";
    CHECK_EQ(copied_message.content,  std::string("hello"));
    CHECK_EQ(original_message.content, std::string("world"));
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 2 — Integration tests (require a real model)
// ─────────────────────────────────────────────────────────────────────────────

static void test_generate(AIModel& model) {
    test::section("AIModel::Generate");

    std::string reply;
    CHECK_NOTHROW(reply = model.Generate("What is the capital of France?"));
    CHECK(!reply.empty());

    // Blank prompt → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.Generate(""));
    CHECK_THROWS(std::invalid_argument, model.Generate("   "));

    // Temperature out of range → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.Generate("hi", -0.1f));
    CHECK_THROWS(std::invalid_argument, model.Generate("hi",  2.1f));

    // max_tokens < 1 → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.Generate("hi", 0.7f, 0));
}

static void test_embed(AIModel& model) {
    test::section("AIModel::Embed");

    auto first_embedding = model.Embed("hello world");
    CHECK(!first_embedding.empty());

    // Same text twice → identical vector (served from cache).
    auto cached_embedding = model.Embed("hello world");
    CHECK_EQ(first_embedding, cached_embedding);

    // use_cache=false still returns a valid vector.
    auto uncached_embedding = model.Embed("hello world", /*use_cache=*/false);
    CHECK(!uncached_embedding.empty());

    // Blank text → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.Embed(""));
    CHECK_THROWS(std::invalid_argument, model.Embed("  "));
}

static void test_similarity(AIModel& model) {
    test::section("AIModel::Similarity");

    float similarity_score = model.Similarity("cat", "kitten");
    CHECK_GE(similarity_score, -1.0f);
    CHECK_GE(1.0f, similarity_score);

    // Self-similarity ≈ 1.0
    float self_similarity_score = model.Similarity("dog", "dog");
    CHECK_GE(self_similarity_score, 0.99f);

    // Semantically related pair should score higher than unrelated pair.
    float related_similarity_score   = model.Similarity("car", "automobile");
    float unrelated_similarity_score = model.Similarity("car", "ocean");
    CHECK_GT(related_similarity_score, unrelated_similarity_score);

    // Blank inputs → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.Similarity("", "word"));
    CHECK_THROWS(std::invalid_argument, model.Similarity("word", ""));
}

static void test_search(AIModel& model) {
    test::section("AIModel::Search");

    std::vector<std::string> candidate_labels = {"sports car", "bicycle", "cargo ship"};
    std::vector<std::string> candidate_texts  = {
        "a high-speed automobile built for racing",
        "a human-powered two-wheeled vehicle",
        "a large vessel that transports goods across oceans"
    };

    auto top_two_results = model.Search("fast racing car", candidate_labels, candidate_texts, 2);
    CHECK_EQ(static_cast<int>(top_two_results.size()), 2);
    // Best match should be "sports car".
    CHECK_EQ(top_two_results[0].second, std::string("sports car"));
    // Results must be sorted best-first.
    CHECK_GE(top_two_results[0].first, top_two_results[1].first);

    // top_n larger than list → returns all.
    auto all_results = model.Search("vehicle", candidate_labels, candidate_texts, 10);
    CHECK_EQ(static_cast<int>(all_results.size()), 3);

    // Validation errors
    CHECK_THROWS(std::invalid_argument, model.Search("", candidate_labels, candidate_texts));
    CHECK_THROWS(std::invalid_argument,
                 model.Search("q", {"a"}, {"x", "y"}));       // size mismatch
    CHECK_THROWS(std::invalid_argument,
                 model.Search("q", candidate_labels, candidate_texts, 0));  // top_n < 1
}

static void test_convo_basic(AIModel& model) {
    test::section("AIConvo::Chat — basic");

    AIConvo conversation(model, "You are a concise assistant.");

    std::string first_reply = conversation.Chat("What is 2 + 2?");
    CHECK(!first_reply.empty());

    // History grows: system + (user + assistant) × 1
    CHECK_EQ(static_cast<int>(conversation.GetHistory().size()), 3);

    std::string second_reply = conversation.Chat("Double that answer.");
    CHECK(!second_reply.empty());

    // History: system + 2 × (user + assistant)
    CHECK_EQ(static_cast<int>(conversation.GetHistory().size()), 5);
}

static void test_convo_validation(AIModel& model) {
    test::section("AIConvo::Chat — input validation");

    AIConvo conversation(model);

    // Blank message → invalid_argument; history must NOT grow.
    std::size_t history_size_before = conversation.GetHistory().size();
    CHECK_THROWS(std::invalid_argument, conversation.Chat(""));
    CHECK_THROWS(std::invalid_argument, conversation.Chat("   "));
    CHECK_EQ(conversation.GetHistory().size(), history_size_before);

    // Temperature out of range
    CHECK_THROWS(std::invalid_argument, conversation.Chat("hi", -0.5f));
    CHECK_THROWS(std::invalid_argument, conversation.Chat("hi",  3.0f));
    CHECK_EQ(conversation.GetHistory().size(), history_size_before);

    // max_tokens < 1
    CHECK_THROWS(std::invalid_argument, conversation.Chat("hi", 0.7f, 0));
    CHECK_EQ(conversation.GetHistory().size(), history_size_before);
}

static void test_convo_clear_history(AIModel& model) {
    test::section("AIConvo::ClearHistory");

    AIConvo conversation(model, "You are a test bot.");
    conversation.Chat("Hello!");

    // History has system + user + assistant.
    CHECK_GT(static_cast<int>(conversation.GetHistory().size()), 1);

    conversation.ClearHistory();

    // Only the system message remains.
    auto cleared_history = conversation.GetHistory();
    CHECK_EQ(static_cast<int>(cleared_history.size()), 1);
    CHECK_EQ(cleared_history[0].role, Role::System);

    // Conversation still works after clearing.
    std::string reply_after_clear = conversation.Chat("Are you still there?");
    CHECK(!reply_after_clear.empty());
}

static void test_convo_get_history_is_copy(AIModel& model) {
    test::section("AIConvo::GetHistory returns a copy");

    AIConvo conversation(model);
    auto mutable_copy = conversation.GetHistory();
    mutable_copy.push_back({Role::User, "injected"});  // mutate the copy

    auto fresh_copy = conversation.GetHistory();
    // Internal history should be unchanged (only system message).
    CHECK_EQ(static_cast<int>(fresh_copy.size()), 1);
}

static void test_convo_title(AIModel& model) {
    test::section("AIConvo::title management");

    AIConvo conversation(model);

    // No title before first chat.
    CHECK(!conversation.GetTitle().has_value());

    // Manual title before first chat.
    conversation.SetTitle("My Test Chat");
    CHECK_EQ(conversation.GetTitle().value(), std::string("My Test Chat"));

    // Blank title → invalid_argument.
    CHECK_THROWS(std::invalid_argument, conversation.SetTitle(""));
    CHECK_THROWS(std::invalid_argument, conversation.SetTitle("   "));

    // Auto-generated title: start a fresh conversation.
    AIConvo second_conversation(model);
    second_conversation.Chat("Tell me about the solar system.");
    // After first chat, title should be set (auto-generated).
    CHECK(second_conversation.GetTitle().has_value());
    CHECK(!second_conversation.GetTitle().value().empty());

    // Title is not regenerated on subsequent turns.
    std::string original_title = second_conversation.GetTitle().value();
    second_conversation.Chat("What is Jupiter?");
    CHECK_EQ(second_conversation.GetTitle().value(), original_title);
}

static void test_convo_persistence(AIModel& model) {
    test::section("AIConvo save/load persistence");

    AIConvo source_conversation(model, "You are a test assistant.");
    source_conversation.Chat("What is the tallest mountain on Earth?");
    source_conversation.SetTitle("Mountain Chat");

    // ── Save ──────────────────────────────────────────────────────────────────
    std::string saved_file_path = source_conversation.SaveHistory(
        "/tmp/convo_test_persist.json");
    CHECK(!saved_file_path.empty());

    // ── Load into a fresh conversation ────────────────────────────────────────
    AIConvo destination_conversation(model);
    CHECK_NOTHROW(destination_conversation.LoadHistory(saved_file_path));

    // History and title must match the source.
    auto source_history      = source_conversation.GetHistory();
    auto destination_history = destination_conversation.GetHistory();
    CHECK_EQ(source_history.size(), destination_history.size());
    for (std::size_t i = 0; i < source_history.size(); ++i) {
        CHECK_EQ(source_history[i].role,    destination_history[i].role);
        CHECK_EQ(source_history[i].content, destination_history[i].content);
    }
    CHECK_EQ(destination_conversation.GetTitle().value(), std::string("Mountain Chat"));

    // Loaded conversation can continue.
    std::string continuation_reply = destination_conversation.Chat("How tall is it in feet?");
    CHECK(!continuation_reply.empty());

    // ── Auto-generated filename ───────────────────────────────────────────────
    AIConvo autoname_conversation(model);
    autoname_conversation.Chat("Testing auto filename.");
    std::string auto_generated_path = autoname_conversation.SaveHistory();
    CHECK(!auto_generated_path.empty());
    std::ifstream auto_file_check(auto_generated_path);
    CHECK(auto_file_check.is_open());
    std::remove(auto_generated_path.c_str());

    // ── Error: file does not exist ────────────────────────────────────────────
    CHECK_THROWS(std::runtime_error,
                 destination_conversation.LoadHistory("/tmp/convo_nonexistent_file.json"));

    // ── Error: malformed JSON ─────────────────────────────────────────────────
    std::string bad_json_path = write_temp_file("{not valid json}", "bad.json");
    CHECK_THROWS(std::invalid_argument, destination_conversation.LoadHistory(bad_json_path));
    std::remove(bad_json_path.c_str());

    // ── Error: missing messages array ─────────────────────────────────────────
    std::string missing_messages_path = write_temp_file("{\"title\": null}", "no_msgs.json");
    CHECK_THROWS(std::invalid_argument,
                 destination_conversation.LoadHistory(missing_messages_path));
    std::remove(missing_messages_path.c_str());

    // ── Error: unknown role ───────────────────────────────────────────────────
    std::string unknown_role_path = write_temp_file(
        R"({"title":null,"messages":[{"role":"robot","content":"hi"}]})",
        "bad_role.json");
    CHECK_THROWS(std::invalid_argument,
                 destination_conversation.LoadHistory(unknown_role_path));
    std::remove(unknown_role_path.c_str());

    // ── Error: missing content field ──────────────────────────────────────────
    std::string missing_content_path = write_temp_file(
        R"({"title":null,"messages":[{"role":"user"}]})",
        "no_content.json");
    CHECK_THROWS(std::invalid_argument,
                 destination_conversation.LoadHistory(missing_content_path));
    std::remove(missing_content_path.c_str());
}

static void test_convo_system_prompt_validation(AIModel& model) {
    test::section("AIConvo construction validation");

    // Blank system prompt → invalid_argument.
    CHECK_THROWS(std::invalid_argument, AIConvo(model, ""));
    CHECK_THROWS(std::invalid_argument, AIConvo(model, "   "));
}

static void test_multiple_convos_independent(AIModel& model) {
    test::section("Multiple AIConvo objects are independent");

    AIConvo history_conversation(model, "You are a history expert.");
    AIConvo cooking_conversation(model, "You are a cooking expert.");

    history_conversation.Chat("Tell me about the Roman Empire.");
    cooking_conversation.Chat("How do I make pasta?");

    // Each conversation has its own separate history.
    for (const auto& history_entry : history_conversation.GetHistory()) {
        if (history_entry.role == Role::System)
            CHECK_EQ(history_entry.content, std::string("You are a history expert."));
    }
    for (const auto& history_entry : cooking_conversation.GetHistory()) {
        if (history_entry.role == Role::System)
            CHECK_EQ(history_entry.content, std::string("You are a cooking expert."));
    }
}

static void test_embed_cache(AIModel& model) {
    test::section("Embedding cache");

    model.ClearEmbedCache();

    auto first_embedding  = model.Embed("test caching");   // populates cache
    auto second_embedding = model.Embed("test caching");   // served from cache
    CHECK_EQ(first_embedding, second_embedding);

    // use_cache=false bypasses cache but still returns a valid vector.
    auto uncached_embedding = model.Embed("test caching", /*use_cache=*/false);
    CHECK(!uncached_embedding.empty());

    // clear_embed_cache wipes the cache.
    model.ClearEmbedCache();
    // Next call recomputes from scratch (should still return a valid vector).
    auto recomputed_embedding = model.Embed("test caching");
    CHECK(!recomputed_embedding.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::cout << "convo test suite\n";

    // ── Part 1: pure unit tests ───────────────────────────────────────────────
    test_role_helpers();
    test_message_struct();
    
    // ── Part 2: integration tests (require a model path) ─────────────────────
    if (argc < 2) {
        std::cout << "\nNo model path provided — skipping integration tests.\n"
                  << "Usage: " << argv[0] << " /path/to/model.gguf\n";
    } 
    else {
        const std::string model_path = argv[1];
        std::cout << "\nLoading model: " << model_path << "\n";

        try {
            AIModel model(model_path);
            std::cout << "Model loaded.\n";

            test_generate(model);
            test_embed(model);
            test_similarity(model);
            test_search(model);
            test_convo_basic(model);
            test_convo_validation(model);
            test_convo_clear_history(model);
            test_convo_get_history_is_copy(model);
            test_convo_title(model);
            test_convo_persistence(model);
            test_convo_system_prompt_validation(model);
            test_multiple_convos_independent(model);
            test_embed_cache(model);

        } catch (const std::exception& error) {
            std::cerr << "\nFailed to load model: " << error.what() << "\n";
            std::cerr << "Integration tests skipped.\n";
        }
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
