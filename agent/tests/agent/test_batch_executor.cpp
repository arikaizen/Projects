// test_batch_executor.cpp — BatchExecutor direct tests (req 33, worked example 10)
//
// Covers:
//   - 3 independent EchoActions: can run concurrently, peak concurrency ≥ 2
//   - Results are in declared order regardless of completion order
//   - Chain a→b→c (b refs $a, c refs $b): sequential execution (peak concurrency = 1)

#include "test_helper.hpp"
#include "agent/batch_executor.hpp"
#include "agent/action.hpp"
#include <atomic>
#include <algorithm>

// ── Timed EchoAction: tracks peak concurrency ─────────────────────────────────

struct TimedAction : agent::Action {
    std::atomic<int>& current;
    std::atomic<int>& peak;
    std::chrono::milliseconds delay;

    TimedAction(std::string id, nlohmann::json inp,
                std::atomic<int>& cur, std::atomic<int>& pk,
                std::chrono::milliseconds d = std::chrono::milliseconds(20))
        : agent::Action(std::move(id), "TimedAction", std::move(inp))
        , current(cur), peak(pk), delay(d) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        int c = ++current;
        // Update peak atomically
        int expected = peak.load();
        while (c > expected && !peak.compare_exchange_weak(expected, c)) {}

        std::this_thread::sleep_for(delay);
        --current;

        auto resolved = ctx.resolveReferences(inputs);
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = resolved;
        r.timestamp = std::chrono::system_clock::now();
        r.duration  = delay;
        return r;
    }
};

// ── Build a raw AgentContext for BatchExecutor tests ──────────────────────────

static std::unique_ptr<agent::AgentContext> makeBatchCtx(const std::string& id = "be_test") {
    auto bus     = std::make_shared<agent::EventBus>();
    auto bb      = std::make_shared<agent::Blackboard>(bus.get());
    auto llm     = std::make_shared<agent::MockLLMClient>(
        [](const agent::LLMClient::Request&) {
            return agent::LLMClient::Response{"[]", true, "", {}};
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
    cfg.task     = "batch test";

    return std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);
}

int main() {
    std::cout << "=== test_batch_executor ===\n";

    agent::ThreadPool pool(6);  // Large enough to not bottleneck concurrency tests

    // ── Section 1: 3 independent items — concurrency ≥ 2 ─────────────────────
    test::section("3 independent actions: peak concurrency >= 2");
    {
        std::atomic<int> current{0};
        std::atomic<int> peak{0};

        auto ctx = makeBatchCtx("be_concurrent");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<TimedAction>(
            "a", nlohmann::json::object(), current, peak));
        batch.push_back(std::make_unique<TimedAction>(
            "b", nlohmann::json::object(), current, peak));
        batch.push_back(std::make_unique<TimedAction>(
            "c", nlohmann::json::object(), current, peak));

        agent::BatchExecutor executor(pool);
        std::vector<agent::WorkResult> results;
        CHECK_NOTHROW(results = executor.execute(std::move(batch), *ctx));

        CHECK_EQ(static_cast<int>(results.size()), 3);
        for (const auto& r : results) { CHECK(r.success); }

        // Peak concurrency should be at least 2 (all 3 can run at once)
        CHECK_GE(peak.load(), 2);
    }

    // ── Section 2: Results in declared order regardless of completion ─────────
    test::section("results in declared order (a, b, c)");
    {
        std::atomic<int> cur{0}, pk{0};
        auto ctx = makeBatchCtx("be_order");

        // a: fast, b: slow, c: fast — declared order a,b,c must be preserved
        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<TimedAction>(
            "ordered_a", nlohmann::json::object(), cur, pk,
            std::chrono::milliseconds(5)));
        batch.push_back(std::make_unique<TimedAction>(
            "ordered_b", nlohmann::json::object(), cur, pk,
            std::chrono::milliseconds(30)));
        batch.push_back(std::make_unique<TimedAction>(
            "ordered_c", nlohmann::json::object(), cur, pk,
            std::chrono::milliseconds(5)));

        agent::BatchExecutor executor(pool);
        auto results = executor.execute(std::move(batch), *ctx);

        CHECK_EQ(static_cast<int>(results.size()), 3);
        CHECK_EQ(results[0].item_id, std::string("ordered_a"));
        CHECK_EQ(results[1].item_id, std::string("ordered_b"));
        CHECK_EQ(results[2].item_id, std::string("ordered_c"));

        // History must also be in declared order
        const auto& hist = ctx->history();
        CHECK_GE(static_cast<int>(hist.size()), 3);
        bool a_before_b = false, b_before_c = false;
        int ia=-1, ib=-1, ic=-1;
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            if (hist[i].item_id == "ordered_a") ia = i;
            if (hist[i].item_id == "ordered_b") ib = i;
            if (hist[i].item_id == "ordered_c") ic = i;
        }
        if (ia >= 0 && ib >= 0) a_before_b = ia < ib;
        if (ib >= 0 && ic >= 0) b_before_c = ib < ic;
        CHECK(a_before_b);
        CHECK(b_before_c);
    }

    // ── Section 3: Chain a→b→c — must run sequentially (peak = 1) ────────────
    test::section("chain a->b->c: sequential execution (peak concurrency = 1)");
    {
        std::atomic<int> cur{0}, pk{0};
        auto ctx = makeBatchCtx("be_chain");

        // Seed history with nothing. b references $chain_a, c references $chain_b.
        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<TimedAction>(
            "chain_a", nlohmann::json::object(), cur, pk,
            std::chrono::milliseconds(10)));
        batch.push_back(std::make_unique<TimedAction>(
            "chain_b", nlohmann::json({{"ref_a", "$chain_a"}}), cur, pk,
            std::chrono::milliseconds(10)));
        batch.push_back(std::make_unique<TimedAction>(
            "chain_c", nlohmann::json({{"ref_b", "$chain_b"}}), cur, pk,
            std::chrono::milliseconds(10)));

        agent::BatchExecutor executor(pool);
        std::vector<agent::WorkResult> results;
        CHECK_NOTHROW(results = executor.execute(std::move(batch), *ctx));

        CHECK_EQ(static_cast<int>(results.size()), 3);
        for (const auto& r : results) { CHECK(r.success); }

        // All sequential: peak concurrency should be 1
        CHECK_EQ(pk.load(), 1);

        // Order preserved: chain_a, chain_b, chain_c
        const auto& hist = ctx->history();
        int ia=-1, ib=-1, ic=-1;
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            if (hist[i].item_id == "chain_a") ia = i;
            if (hist[i].item_id == "chain_b") ib = i;
            if (hist[i].item_id == "chain_c") ic = i;
        }
        if (ia >= 0 && ib >= 0) CHECK(ia < ib);
        if (ib >= 0 && ic >= 0) CHECK(ib < ic);
    }

    // ── Section 4: Failed dependency → dependents skipped ─────────────────────
    test::section("failed item causes dependent to be skipped");
    {
        struct FailingAction : agent::Action {
            FailingAction(std::string id, nlohmann::json inp)
                : agent::Action(std::move(id), "FailingAction", std::move(inp)) {}
            agent::WorkResult execute(agent::AgentContext&) override {
                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = false;
                r.error     = "intentional failure";
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        auto ctx = makeBatchCtx("be_fail");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<FailingAction>("fail_a", nlohmann::json{}));

        // fail_b depends on fail_a (via $fail_a reference)
        std::atomic<int> cur{0}, pk{0};
        batch.push_back(std::make_unique<TimedAction>(
            "fail_b", nlohmann::json({{"dep", "$fail_a"}}), cur, pk));

        agent::BatchExecutor executor(pool);
        auto results = executor.execute(std::move(batch), *ctx);

        CHECK_EQ(static_cast<int>(results.size()), 2);
        CHECK(!results[0].success);  // fail_a failed
        CHECK(!results[1].success);  // fail_b skipped
        CHECK(!results[1].skipped_reason.empty());  // has skip reason
    }

    // ── Section 5: Cancellation stops new items ───────────────────────────────
    test::section("cancellation_flag prevents new items from starting");
    {
        std::atomic<int> cur{0}, pk{0};
        auto ctx = makeBatchCtx("be_cancel");

        // Set cancellation before execute
        ctx->cancellation_flag.store(true);

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<TimedAction>(
            "ca1", nlohmann::json::object(), cur, pk));
        batch.push_back(std::make_unique<TimedAction>(
            "ca2", nlohmann::json::object(), cur, pk));

        agent::BatchExecutor executor(pool);
        auto results = executor.execute(std::move(batch), *ctx);

        // All items should be marked as failed/cancelled, none as success
        CHECK_EQ(static_cast<int>(results.size()), 2);
        for (const auto& r : results) {
            CHECK(!r.success);  // cancelled items are not successful
        }
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
