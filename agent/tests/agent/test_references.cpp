// test_references.cpp — AgentContext::resolveReferences() unit tests
//
// Covers:
//   - "$id"       resolves to the full output JSON of that history entry
//   - "$id.field" resolves to a specific field of the output
//   - Unknown ref throws std::runtime_error
//   - Nested JSON with refs resolves correctly
//   - Non-ref strings are left untouched

#include "test_helper.hpp"

// ── Build a context and seed it with synthetic history ────────────────────────

static std::unique_ptr<agent::AgentContext> makeCtxWithHistory() {
    auto bus     = std::make_shared<agent::EventBus>();
    auto bb      = std::make_shared<agent::Blackboard>(bus.get());
    auto llm     = std::make_shared<agent::MockLLMClient>(
        [](const agent::LLMClient::Request&) { return agent::LLMClient::Response{"[]",true,""}; });
    auto factory = std::make_shared<agent::WorkFactory>();
    auto loader  = std::make_shared<agent::PromptLoader>("/tmp");
    auto mem     = std::make_shared<agent::NoOpMemoryBackend>();
    auto inbox   = std::make_shared<agent::MessageInbox>();

    // AgentContext stores raw (non-owning) pointers to bus/bb/inbox; keep them
    // alive for the lifetime of the test process (test-only).
    static std::vector<std::shared_ptr<void>> s_keepalive;
    s_keepalive.push_back(bus);
    s_keepalive.push_back(bb);
    s_keepalive.push_back(inbox);

    agent::AgentConfig cfg;
    cfg.agent_id = "ref_test";
    cfg.task     = "test";

    auto ctx = std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);

    // Seed history with two synthetic results
    {
        agent::WorkResult r1;
        r1.item_id   = "step1";
        r1.item_name = "EchoAction";
        r1.item_kind = "Action";
        r1.success   = true;
        r1.output    = {{"value", 42}, {"text", "hello from step1"}};
        r1.timestamp = std::chrono::system_clock::now();
        ctx->recordResult(r1);
    }
    {
        agent::WorkResult r2;
        r2.item_id   = "step2";
        r2.item_name = "EchoAction";
        r2.item_kind = "Action";
        r2.success   = true;
        r2.output    = {{"nested", {{"deep", "treasure"}}}, {"count", 7}};
        r2.timestamp = std::chrono::system_clock::now();
        ctx->recordResult(r2);
    }

    return ctx;
}

int main() {
    std::cout << "=== test_references ===\n";

    auto ctx = makeCtxWithHistory();

    // ── Section 1: "$id" resolves to full output ──────────────────────────────
    test::section("$id resolves to full output JSON");
    {
        nlohmann::json inputs = {{"ref", "$step1"}};
        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx->resolveReferences(inputs));
        // resolved["ref"] should equal the full output of step1
        CHECK(resolved.contains("ref"));
        if (resolved.contains("ref")) {
            CHECK(resolved["ref"].contains("value"));
            CHECK(resolved["ref"].contains("text"));
            CHECK_EQ(resolved["ref"]["value"].get<int>(), 42);
            CHECK_EQ(resolved["ref"]["text"].get<std::string>(), std::string("hello from step1"));
        }
    }

    // ── Section 2: "$id.field" resolves to a specific field ───────────────────
    test::section("$id.field resolves to specific field");
    {
        nlohmann::json inputs = {{"count", "$step2.count"}};
        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx->resolveReferences(inputs));
        CHECK(resolved.contains("count"));
        if (resolved.contains("count")) {
            CHECK_EQ(resolved["count"].get<int>(), 7);
        }
    }

    // ── Section 3: Unknown ref throws ─────────────────────────────────────────
    test::section("unknown ref throws runtime_error");
    {
        nlohmann::json inputs = {{"bad", "$nonexistent_id"}};
        CHECK_THROW(ctx->resolveReferences(inputs));
    }

    // ── Section 4: Nested JSON with refs resolves correctly ───────────────────
    test::section("nested JSON resolves refs at any depth");
    {
        nlohmann::json inputs = {
            {"outer", {
                {"inner_val", "$step1.value"},
                {"inner_text", "$step1.text"},
                {"literal", "no-change"}
            }}
        };
        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx->resolveReferences(inputs));
        CHECK(resolved.contains("outer"));
        if (resolved.contains("outer")) {
            CHECK_EQ(resolved["outer"]["inner_val"].get<int>(), 42);
            CHECK_EQ(resolved["outer"]["inner_text"].get<std::string>(), std::string("hello from step1"));
            CHECK_EQ(resolved["outer"]["literal"].get<std::string>(), std::string("no-change"));
        }
    }

    // ── Section 5: Non-ref strings are left untouched ─────────────────────────
    test::section("non-ref strings unchanged");
    {
        nlohmann::json inputs = {
            {"plain",      "just a string"},
            {"num",        123},
            {"flag",       true},
            {"dollar_mid", "prefix$step1suffix"}  // only starts-with-$ is a ref
        };
        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx->resolveReferences(inputs));
        CHECK_EQ(resolved["plain"].get<std::string>(), std::string("just a string"));
        CHECK_EQ(resolved["num"].get<int>(), 123);
        CHECK_EQ(resolved["flag"].get<bool>(), true);
        // "prefix$step1suffix" does NOT start with '$' alone, so unchanged
        CHECK_EQ(resolved["dollar_mid"].get<std::string>(), std::string("prefix$step1suffix"));
    }

    // ── Section 6: Array inputs with refs resolve correctly ──────────────────
    test::section("array inputs with refs");
    {
        nlohmann::json inputs = {
            {"items", nlohmann::json::array({"$step1.value", "literal", "$step2.count"})}
        };
        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx->resolveReferences(inputs));
        CHECK(resolved.contains("items"));
        if (resolved.contains("items") && resolved["items"].is_array()) {
            CHECK_EQ(static_cast<int>(resolved["items"].size()), 3);
            CHECK_EQ(resolved["items"][0].get<int>(), 42);
            CHECK_EQ(resolved["items"][1].get<std::string>(), std::string("literal"));
            CHECK_EQ(resolved["items"][2].get<int>(), 7);
        }
    }

    // ── Section 7: "$id.field" on unknown field throws ────────────────────────
    test::section("$id.unknown_field throws");
    {
        nlohmann::json inputs = {{"bad_field", "$step1.nonexistent_field"}};
        CHECK_THROW(ctx->resolveReferences(inputs));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
