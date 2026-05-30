// test_cycle_detection.cpp — Cycle detection (req 15, worked example 11)
//
// Covers:
//   - BatchExecutor detects a direct cycle (a deps b, b deps a) and throws
//   - BatchExecutor detects a self-loop
//   - ReasonStage-equivalent: plan where x refs $y and y refs $x → error
//   - Acyclic plan succeeds without error

#include "test_helper.hpp"
#include "agent/batch_executor.hpp"
#include "agent/action.hpp"
#include "agent/agent.hpp"

// ── Helper: make a batch context ─────────────────────────────────────────────

static std::unique_ptr<agent::AgentContext> makeCycleCtx(const std::string& id) {
    auto bus     = std::make_shared<agent::EventBus>();
    auto bb      = std::make_shared<agent::Blackboard>(bus.get());
    auto llm     = std::make_shared<agent::MockLLMClient>(
        [](const agent::LLMClient::Request&) {
            return agent::LLMClient::Response{"[]", true, ""};
        });
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
    cfg.agent_id = id;
    cfg.task     = "cycle test";

    return std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);
}

// Minimal action that is used only for its dependency computation
struct DepAction : agent::Action {
    DepAction(std::string id, nlohmann::json inp)
        : agent::Action(std::move(id), "DepAction", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext&) override {
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};

int main() {
    std::cout << "=== test_cycle_detection ===\n";

    agent::ThreadPool pool(4);

    // ── Section 1: Direct cycle a→b, b→a throws ──────────────────────────────
    test::section("direct cycle a<->b throws std::runtime_error");
    {
        auto ctx = makeCycleCtx("cycle_ab");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        // a has inputs referencing $b; b has inputs referencing $a
        batch.push_back(std::make_unique<DepAction>("cyc_a", nlohmann::json{{"dep","$cyc_b"}}));
        batch.push_back(std::make_unique<DepAction>("cyc_b", nlohmann::json{{"dep","$cyc_a"}}));

        agent::BatchExecutor executor(pool);
        CHECK_THROW(executor.execute(std::move(batch), *ctx));
    }

    // ── Section 2: Three-node cycle a→b→c→a throws ────────────────────────────
    test::section("three-node cycle a->b->c->a throws");
    {
        auto ctx = makeCycleCtx("cycle_abc");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<DepAction>("tc_a", nlohmann::json{{"dep","$tc_c"}}));
        batch.push_back(std::make_unique<DepAction>("tc_b", nlohmann::json{{"dep","$tc_a"}}));
        batch.push_back(std::make_unique<DepAction>("tc_c", nlohmann::json{{"dep","$tc_b"}}));

        agent::BatchExecutor executor(pool);
        CHECK_THROW(executor.execute(std::move(batch), *ctx));
    }

    // ── Section 3: Unresolvable dependency (not in history, not in batch) ──────
    test::section("unresolvable dependency throws");
    {
        auto ctx = makeCycleCtx("cycle_unresolvable");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        // References "ghost" which exists neither in history nor in batch
        batch.push_back(std::make_unique<DepAction>("ud_a", nlohmann::json{{"dep","$ghost"}}));

        agent::BatchExecutor executor(pool);
        CHECK_THROW(executor.execute(std::move(batch), *ctx));
    }

    // ── Section 4: Acyclic batch succeeds ────────────────────────────────────
    test::section("acyclic batch (a->b->c) succeeds");
    {
        auto ctx = makeCycleCtx("cycle_acyclic");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<DepAction>("ac_a", nlohmann::json{}));
        batch.push_back(std::make_unique<DepAction>("ac_b", nlohmann::json{{"dep","$ac_a"}}));
        batch.push_back(std::make_unique<DepAction>("ac_c", nlohmann::json{{"dep","$ac_b"}}));

        agent::BatchExecutor executor(pool);
        std::vector<agent::WorkResult> results;
        CHECK_NOTHROW(results = executor.execute(std::move(batch), *ctx));
        CHECK_EQ(static_cast<int>(results.size()), 3);
        for (const auto& r : results) { CHECK(r.success); }
    }

    // ── Section 5: MockReasonStage-equivalent: cycle in plan → failure ─────────
    // When the "LLM" returns a plan where x refs $y and y refs $x,
    // a ValidatingPlanStage should return success=false with a cycle error.
    test::section("cycle in LLM plan causes stage to fail");
    {
        // Cyclic plan: x references $y, y references $x
        // Since both are in the same plan batch, this is a cycle.
        // We test it directly via BatchExecutor since that's where the detection lives.
        auto ctx = makeCycleCtx("cycle_plan");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<DepAction>("cyc_x", nlohmann::json{{"dep","$cyc_y"}}));
        batch.push_back(std::make_unique<DepAction>("cyc_y", nlohmann::json{{"dep","$cyc_x"}}));

        agent::BatchExecutor executor(pool);
        bool threw = false;
        std::string error_msg;
        try {
            executor.execute(std::move(batch), *ctx);
        } catch (const std::exception& ex) {
            threw = true;
            error_msg = ex.what();
        }
        CHECK(threw);
        // Error message should mention cycle
        CHECK(error_msg.find("ycle") != std::string::npos ||
              error_msg.find("ependency") != std::string::npos);
    }

    // ── Section 6: Self-dependency (a refs $a) throws ─────────────────────────
    test::section("self-dependency throws");
    {
        auto ctx = makeCycleCtx("cycle_self");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        // "self_a" references its own output "$self_a" — a self-loop
        batch.push_back(std::make_unique<DepAction>("self_a", nlohmann::json{{"dep","$self_a"}}));

        agent::BatchExecutor executor(pool);
        CHECK_THROW(executor.execute(std::move(batch), *ctx));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
