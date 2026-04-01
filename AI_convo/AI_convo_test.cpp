/**
 * AI_convo_test.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Test suite for AI_convo.hpp / AI_convo.cpp.
 *
 * Structure
 * ─────────
 *   Part 1  — Pure unit tests (no model required).
 *             Tests Role helpers, Message struct, input validation logic,
 *             history management, and JSON save/load round-trips.
 *
 *   Part 2  — Integration tests (require a real GGUF model on disk).
 *             Pass the model path as the first command-line argument to run:
 *               ./AI_convo_test /path/to/model.gguf
 *
 * Build (example, adjust paths for your llama.cpp install):
 *   g++ -std=c++17 -O2 \
 *       -I/path/to/llama.cpp/include \
 *       -I/path/to/nlohmann \
 *       AI_convo.cpp AI_convo_test.cpp \
 *       -L/path/to/llama.cpp/build -lllama \
 *       -lpthread -ldl \
 *       -o AI_convo_test
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "AI_convo.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

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
#define CHECK_THROWS(type, expr)                                          \
    do {                                                                   \
        bool _threw = false;                                               \
        try { (void)(expr); }                                              \
        catch (const type&) { _threw = true; }                            \
        catch (...) {}                                                     \
        test::record(_threw, "throws " #type ": " #expr, __FILE__, __LINE__); \
    } while (0)

/** Verify that EXPR does NOT throw. */
#define CHECK_NOTHROW(expr)                                               \
    do {                                                                   \
        bool _threw = false;                                               \
        try { (void)(expr); }                                              \
        catch (...) { _threw = true; }                                     \
        test::record(!_threw, "no-throw: " #expr, __FILE__, __LINE__);    \
    } while (0)

// ─────────────────────────────────────────────────────────────────────────────
// Helper: write a string to a temporary file; return the path
// ─────────────────────────────────────────────────────────────────────────────

static std::string write_tmp(const std::string& contents, const std::string& suffix = ".json") {
    std::string path = "/tmp/ai_convo_test_" + suffix;
    std::ofstream ofs(path);
    ofs << contents;
    return path;
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 1 — Pure unit tests (no model needed)
// ─────────────────────────────────────────────────────────────────────────────

static void test_role_helpers() {
    test::section("Role helpers");

    // role_to_str
    CHECK_EQ(role_to_str(Role::System),    std::string("system"));
    CHECK_EQ(role_to_str(Role::User),      std::string("user"));
    CHECK_EQ(role_to_str(Role::Assistant), std::string("assistant"));

    // role_from_str round-trips
    CHECK_EQ(role_from_str("system"),    Role::System);
    CHECK_EQ(role_from_str("user"),      Role::User);
    CHECK_EQ(role_from_str("assistant"), Role::Assistant);

    // Unknown string throws
    CHECK_THROWS(std::invalid_argument, role_from_str("robot"));
    CHECK_THROWS(std::invalid_argument, role_from_str(""));
    CHECK_THROWS(std::invalid_argument, role_from_str("System"));  // case sensitive
}

static void test_message_struct() {
    test::section("Message struct");

    Message m{Role::User, "hello"};
    CHECK_EQ(m.role,    Role::User);
    CHECK_EQ(m.content, std::string("hello"));

    // Copy
    Message m2 = m;
    CHECK_EQ(m2.role,    Role::User);
    CHECK_EQ(m2.content, std::string("hello"));

    // Mutate original does not affect copy
    m.content = "world";
    CHECK_EQ(m2.content, std::string("hello"));
    CHECK_EQ(m.content,  std::string("world"));
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON save/load unit tests (no inference; we manually build the JSON)
// ─────────────────────────────────────────────────────────────────────────────

// We test load_history by writing a valid JSON file and loading it.
// We need an AIModel to construct AIConvo — so these tests are in the
// integration section.  However, we can test the JSON format parsing by
// examining the save output from a freshly constructed AIConvo loaded from
// hand-crafted JSON.  Those tests live in test_persistence() below and run
// only when a model is available.
//
// We CAN test that load_history rejects malformed input without running
// inference, provided we have an AIModel (which requires a real model file).
// Those tests are grouped in the integration section.

// ─────────────────────────────────────────────────────────────────────────────
// PART 2 — Integration tests (require a real model)
// ─────────────────────────────────────────────────────────────────────────────

static void test_generate(AIModel& model) {
    test::section("AIModel::generate");

    // Normal call returns a non-blank string.
    std::string reply;
    CHECK_NOTHROW(reply = model.generate("What is the capital of France?"));
    CHECK(!reply.empty());

    // Blank prompt → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.generate(""));
    CHECK_THROWS(std::invalid_argument, model.generate("   "));

    // Temperature out of range → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.generate("hi", -0.1f));
    CHECK_THROWS(std::invalid_argument, model.generate("hi",  2.1f));

    // max_tokens < 1 → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.generate("hi", 0.7f, 0));
}

static void test_embed(AIModel& model) {
    test::section("AIModel::embed");

    auto v = model.embed("hello world");
    CHECK(!v.empty());

    // Same text twice → identical vector (from cache).
    auto v2 = model.embed("hello world");
    CHECK_EQ(v, v2);

    // use_cache=false still returns a valid vector.
    auto v3 = model.embed("hello world", /*use_cache=*/false);
    CHECK(!v3.empty());

    // Blank text → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.embed(""));
    CHECK_THROWS(std::invalid_argument, model.embed("  "));
}

static void test_similarity(AIModel& model) {
    test::section("AIModel::similarity");

    float s = model.similarity("cat", "kitten");
    CHECK_GE(s, -1.0f);
    CHECK_GE(1.0f, s);

    // Self-similarity ≈ 1.0
    float self = model.similarity("dog", "dog");
    CHECK_GE(self, 0.99f);

    // Semantically related pair should score higher than unrelated pair.
    float related   = model.similarity("car",   "automobile");
    float unrelated = model.similarity("car",   "ocean");
    CHECK_GT(related, unrelated);

    // Blank inputs → invalid_argument
    CHECK_THROWS(std::invalid_argument, model.similarity("", "word"));
    CHECK_THROWS(std::invalid_argument, model.similarity("word", ""));
}

static void test_search(AIModel& model) {
    test::section("AIModel::search");

    std::vector<std::string> labels = {"sports car", "bicycle", "cargo ship"};
    std::vector<std::string> texts  = {
        "a high-speed automobile built for racing",
        "a human-powered two-wheeled vehicle",
        "a large vessel that transports goods across oceans"
    };

    auto results = model.search("fast racing car", labels, texts, 2);
    CHECK_EQ(static_cast<int>(results.size()), 2);
    // Best match should be "sports car".
    CHECK_EQ(results[0].second, std::string("sports car"));
    // Results must be sorted best-first.
    CHECK_GE(results[0].first, results[1].first);

    // top_n larger than list → returns all.
    auto all = model.search("vehicle", labels, texts, 10);
    CHECK_EQ(static_cast<int>(all.size()), 3);

    // Validation errors
    CHECK_THROWS(std::invalid_argument, model.search("", labels, texts));
    CHECK_THROWS(std::invalid_argument,
                 model.search("q", {"a"}, {"x", "y"}));           // size mismatch
    CHECK_THROWS(std::invalid_argument,
                 model.search("q", labels, texts, 0));             // top_n < 1
}

static void test_convo_basic(AIModel& model) {
    test::section("AIConvo::chat — basic");

    AIConvo convo(model, "You are a concise assistant.");

    std::string r1 = convo.chat("What is 2 + 2?");
    CHECK(!r1.empty());

    // History grows: system + (user + assistant) × 1
    CHECK_EQ(static_cast<int>(convo.get_history().size()), 3);

    std::string r2 = convo.chat("Double that answer.");
    CHECK(!r2.empty());

    // History: system + 2 × (user + assistant)
    CHECK_EQ(static_cast<int>(convo.get_history().size()), 5);
}

static void test_convo_validation(AIModel& model) {
    test::section("AIConvo::chat — input validation");

    AIConvo convo(model);

    // Blank message → invalid_argument; history must NOT grow.
    std::size_t before = convo.get_history().size();
    CHECK_THROWS(std::invalid_argument, convo.chat(""));
    CHECK_THROWS(std::invalid_argument, convo.chat("   "));
    CHECK_EQ(convo.get_history().size(), before);

    // Temperature out of range
    CHECK_THROWS(std::invalid_argument, convo.chat("hi", -0.5f));
    CHECK_THROWS(std::invalid_argument, convo.chat("hi",  3.0f));
    CHECK_EQ(convo.get_history().size(), before);

    // max_tokens < 1
    CHECK_THROWS(std::invalid_argument, convo.chat("hi", 0.7f, 0));
    CHECK_EQ(convo.get_history().size(), before);
}

static void test_convo_clear_history(AIModel& model) {
    test::section("AIConvo::clear_history");

    AIConvo convo(model, "You are a test bot.");
    convo.chat("Hello!");

    // History has system + user + assistant.
    CHECK_GT(static_cast<int>(convo.get_history().size()), 1);

    convo.clear_history();

    // Only the system message remains.
    auto h = convo.get_history();
    CHECK_EQ(static_cast<int>(h.size()), 1);
    CHECK_EQ(h[0].role, Role::System);

    // Conversation still works after clearing.
    std::string reply = convo.chat("Are you still there?");
    CHECK(!reply.empty());
}

static void test_convo_get_history_is_copy(AIModel& model) {
    test::section("AIConvo::get_history returns a copy");

    AIConvo convo(model);
    auto h1 = convo.get_history();
    h1.push_back({Role::User, "injected"});  // mutate the copy

    auto h2 = convo.get_history();
    // Internal history should be unchanged (only system message).
    CHECK_EQ(static_cast<int>(h2.size()), 1);
}

static void test_convo_title(AIModel& model) {
    test::section("AIConvo::title management");

    AIConvo convo(model);

    // No title before first chat.
    CHECK(!convo.get_title().has_value());

    // Manual title before first chat.
    convo.set_title("My Test Chat");
    CHECK_EQ(convo.get_title().value(), std::string("My Test Chat"));

    // Blank title → invalid_argument.
    CHECK_THROWS(std::invalid_argument, convo.set_title(""));
    CHECK_THROWS(std::invalid_argument, convo.set_title("   "));

    // Auto-generated title: start a fresh convo.
    AIConvo convo2(model);
    convo2.chat("Tell me about the solar system.");
    // After first chat, title should be set (auto-generated).
    CHECK(convo2.get_title().has_value());
    CHECK(!convo2.get_title().value().empty());

    // Title is not regenerated on subsequent turns.
    std::string title1 = convo2.get_title().value();
    convo2.chat("What is Jupiter?");
    CHECK_EQ(convo2.get_title().value(), title1);
}

static void test_convo_persistence(AIModel& model) {
    test::section("AIConvo save/load persistence");

    AIConvo src(model, "You are a test assistant.");
    src.chat("What is the tallest mountain on Earth?");
    src.set_title("Mountain Chat");

    // ── Save ──────────────────────────────────────────────────────────────────
    std::string saved_path = src.save_history("/tmp/ai_convo_test_persist.json");
    CHECK(!saved_path.empty());

    // ── Load into a fresh conversation ────────────────────────────────────────
    AIConvo dst(model);
    CHECK_NOTHROW(dst.load_history(saved_path));

    // History and title must match the source.
    auto src_h = src.get_history();
    auto dst_h = dst.get_history();
    CHECK_EQ(src_h.size(), dst_h.size());
    for (std::size_t i = 0; i < src_h.size(); ++i) {
        CHECK_EQ(src_h[i].role,    dst_h[i].role);
        CHECK_EQ(src_h[i].content, dst_h[i].content);
    }
    CHECK_EQ(dst.get_title().value(), std::string("Mountain Chat"));

    // Loaded conversation can continue.
    std::string cont = dst.chat("How tall is it in feet?");
    CHECK(!cont.empty());

    // ── Auto-generated filename ───────────────────────────────────────────────
    AIConvo autoname(model);
    autoname.chat("Testing auto filename.");
    std::string auto_path = autoname.save_history();  // no path given
    CHECK(!auto_path.empty());
    std::ifstream check(auto_path);
    CHECK(check.is_open());
    std::remove(auto_path.c_str());

    // ── Error: file does not exist ────────────────────────────────────────────
    CHECK_THROWS(std::runtime_error,
                 dst.load_history("/tmp/ai_convo_nonexistent_file.json"));

    // ── Error: malformed JSON ─────────────────────────────────────────────────
    std::string bad_path = write_tmp("{not valid json}", "bad.json");
    CHECK_THROWS(std::invalid_argument, dst.load_history(bad_path));
    std::remove(bad_path.c_str());

    // ── Error: missing messages array ────────────────────────────────────────
    std::string no_msgs = write_tmp("{\"title\": null}", "no_msgs.json");
    CHECK_THROWS(std::invalid_argument, dst.load_history(no_msgs));
    std::remove(no_msgs.c_str());

    // ── Error: unknown role ───────────────────────────────────────────────────
    std::string bad_role = write_tmp(
        R"({"title":null,"messages":[{"role":"robot","content":"hi"}]})",
        "bad_role.json");
    CHECK_THROWS(std::invalid_argument, dst.load_history(bad_role));
    std::remove(bad_role.c_str());

    // ── Error: missing content field ─────────────────────────────────────────
    std::string no_content = write_tmp(
        R"({"title":null,"messages":[{"role":"user"}]})",
        "no_content.json");
    CHECK_THROWS(std::invalid_argument, dst.load_history(no_content));
    std::remove(no_content.c_str());
}

static void test_convo_system_prompt_validation(AIModel& model) {
    test::section("AIConvo construction validation");

    // Blank system prompt → invalid_argument.
    CHECK_THROWS(std::invalid_argument, AIConvo(model, ""));
    CHECK_THROWS(std::invalid_argument, AIConvo(model, "   "));
}

static void test_multiple_convos_independent(AIModel& model) {
    test::section("Multiple AIConvo objects are independent");

    AIConvo c1(model, "You are a history expert.");
    AIConvo c2(model, "You are a cooking expert.");

    c1.chat("Tell me about the Roman Empire.");
    c2.chat("How do I make pasta?");

    // Each conversation has its own separate history.
    for (const auto& m : c1.get_history()) {
        if (m.role == Role::System)
            CHECK_EQ(m.content, std::string("You are a history expert."));
    }
    for (const auto& m : c2.get_history()) {
        if (m.role == Role::System)
            CHECK_EQ(m.content, std::string("You are a cooking expert."));
    }
}

static void test_embed_cache(AIModel& model) {
    test::section("Embedding cache");

    model.clear_embed_cache();

    auto v1 = model.embed("test caching");   // populates cache
    auto v2 = model.embed("test caching");   // served from cache
    CHECK_EQ(v1, v2);

    // use_cache=false bypasses cache but still returns a valid vector.
    auto v3 = model.embed("test caching", /*use_cache=*/false);
    CHECK(!v3.empty());

    // clear_embed_cache wipes the cache.
    model.clear_embed_cache();
    // Next call recomputes from scratch (should still return a valid vector).
    auto v4 = model.embed("test caching");
    CHECK(!v4.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::cout << "AI_convo test suite\n";

    // ── Part 1: pure unit tests ───────────────────────────────────────────────
    test_role_helpers();
    test_message_struct();

    // ── Part 2: integration tests (require a model path) ─────────────────────
    if (argc < 2) {
        std::cout << "\nNo model path provided — skipping integration tests.\n"
                  << "Usage: " << argv[0] << " /path/to/model.gguf\n";
    } else {
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

        } catch (const std::exception& e) {
            std::cerr << "\nFailed to load model: " << e.what() << "\n";
            std::cerr << "Integration tests skipped.\n";
        }
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
