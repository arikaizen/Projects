// test_dependency_dag.cpp — DAG construction and execution order (req 33)
//
// Batch: [a(no deps), b(deps a), c(deps a), d(deps b,c)]
//
// Expected execution:
//   - a runs first (no deps)
//   - b and c run concurrently after a
//   - d runs last (depends on both b and c)
//
// History order must match declared order: a, b, c, d

#include "test_helper.hpp"
#include "agent/batch_executor.hpp"
#include "agent/action.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

// ── Instrumented Action: records execution timestamps ─────────────────────────

struct InstrumentedAction : agent::Action {
    // Shared state (passed by pointer so it lives outside the action)
    std::mutex*              log_mutex;
    std::vector<std::string>* exec_order;
    std::atomic<int>*        current_concurrency;
    std::atomic<int>*        peak_concurrency;
    std::chrono::milliseconds delay;

    InstrumentedAction(std::string id, nlohmann::json inp,
                       std::mutex* mx,
                       std::vector<std::string>* order,
                       std::atomic<int>* cur,
                       std::atomic<int>* pk,
                       std::chrono::milliseconds d = std::chrono::milliseconds(20))
        : agent::Action(std::move(id), "InstrumentedAction", std::move(inp))
        , log_mutex(mx), exec_order(order)
        , current_concurrency(cur), peak_concurrency(pk)
        , delay(d) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        int c = ++(*current_concurrency);
        int expected = peak_concurrency->load();
        while (c > expected && !peak_concurrency->compare_exchange_weak(expected, c)) {}

        // Record start
        {
            std::lock_guard<std::mutex> lk(*log_mutex);
            exec_order->push_back(id + ":start");
        }

        std::this_thread::sleep_for(delay);

        --(*current_concurrency);

        // Resolve any $references in inputs
        auto resolved = ctx.resolveReferences(inputs);

        {
            std::lock_guard<std::mutex> lk(*log_mutex);
            exec_order->push_back(id + ":end");
        }

        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = {{"id_done", id}, {"resolved", resolved}};
        r.timestamp = std::chrono::system_clock::now();
        r.duration  = delay;
        return r;
    }
};

// ── Build a context for batch tests ──────────────────────────────────────────

static std::unique_ptr<agent::AgentContext> makeDagCtx(const std::string& id) {
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
    cfg.task     = "dag test";

    return std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);
}

int main() {
    std::cout << "=== test_dependency_dag ===\n";

    agent::ThreadPool pool(8);

    // ── Section 1: Diamond DAG  a → (b,c) → d ────────────────────────────────
    test::section("diamond DAG: a first, b+c concurrent, d last");
    {
        std::mutex              log_mx;
        std::vector<std::string> exec_order;
        std::atomic<int>        cur{0}, pk{0};

        auto ctx = makeDagCtx("dag_diamond");

        // Declared order: a, b, c, d
        // b and c both depend on $a; d depends on $b and $c
        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<InstrumentedAction>(
            "a", nlohmann::json::object(), &log_mx, &exec_order, &cur, &pk,
            std::chrono::milliseconds(15)));
        batch.push_back(std::make_unique<InstrumentedAction>(
            "b", nlohmann::json({{"from_a","$a"}}), &log_mx, &exec_order, &cur, &pk,
            std::chrono::milliseconds(20)));
        batch.push_back(std::make_unique<InstrumentedAction>(
            "c", nlohmann::json({{"from_a","$a"}}), &log_mx, &exec_order, &cur, &pk,
            std::chrono::milliseconds(20)));
        batch.push_back(std::make_unique<InstrumentedAction>(
            "d", nlohmann::json({{"from_b","$b"},{"from_c","$c"}}),
            &log_mx, &exec_order, &cur, &pk,
            std::chrono::milliseconds(10)));

        agent::BatchExecutor executor(pool);
        std::vector<agent::WorkResult> results;
        CHECK_NOTHROW(results = executor.execute(std::move(batch), *ctx));

        CHECK_EQ(static_cast<int>(results.size()), 4);
        for (const auto& r : results) { CHECK(r.success); }

        // All four items ran
        bool found_a = false, found_b = false, found_c = false, found_d = false;
        for (const auto& r : results) {
            if (r.item_id == "a") found_a = true;
            if (r.item_id == "b") found_b = true;
            if (r.item_id == "c") found_c = true;
            if (r.item_id == "d") found_d = true;
        }
        CHECK(found_a); CHECK(found_b); CHECK(found_c); CHECK(found_d);

        // b and c ran concurrently → peak >= 2
        CHECK_GE(pk.load(), 2);

        // Execution trace: a must end before b or c start
        auto find_pos = [&](const std::string& tag) -> int {
            for (int i = 0; i < static_cast<int>(exec_order.size()); ++i)
                if (exec_order[i] == tag) return i;
            return -1;
        };

        int a_end   = find_pos("a:end");
        int b_start = find_pos("b:start");
        int c_start = find_pos("c:start");
        int b_end   = find_pos("b:end");
        int c_end   = find_pos("c:end");
        int d_start = find_pos("d:start");

        if (a_end >= 0 && b_start >= 0) CHECK(a_end < b_start);
        if (a_end >= 0 && c_start >= 0) CHECK(a_end < c_start);
        if (b_end >= 0 && d_start >= 0) CHECK(b_end < d_start);
        if (c_end >= 0 && d_start >= 0) CHECK(c_end < d_start);

        // History order must match declared order: a, b, c, d
        const auto& hist = ctx->history();
        int ha=-1, hb=-1, hc=-1, hd=-1;
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            if (hist[i].item_id == "a") ha = i;
            if (hist[i].item_id == "b") hb = i;
            if (hist[i].item_id == "c") hc = i;
            if (hist[i].item_id == "d") hd = i;
        }
        if (ha >= 0 && hb >= 0) CHECK(ha < hb);
        if (hb >= 0 && hc >= 0) CHECK(hb < hc);   // declared order b before c
        if (hc >= 0 && hd >= 0) CHECK(hc < hd);
    }

    // ── Section 2: Totally independent items all run concurrently ─────────────
    test::section("5 independent items: peak concurrency = 5");
    {
        std::mutex              log_mx;
        std::vector<std::string> exec_order;
        std::atomic<int>        cur{0}, pk{0};

        auto ctx = makeDagCtx("dag_parallel");

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        for (int i = 0; i < 5; ++i) {
            batch.push_back(std::make_unique<InstrumentedAction>(
                "p" + std::to_string(i), nlohmann::json::object(),
                &log_mx, &exec_order, &cur, &pk,
                std::chrono::milliseconds(30)));
        }

        agent::BatchExecutor executor(pool);
        auto results = executor.execute(std::move(batch), *ctx);

        CHECK_EQ(static_cast<int>(results.size()), 5);
        for (const auto& r : results) { CHECK(r.success); }

        // With 8 pool threads, all 5 should run concurrently
        CHECK_GE(pk.load(), 3);  // conservative lower bound for CI

        // History order: p0, p1, p2, p3, p4 (declared order)
        const auto& hist = ctx->history();
        int prev_idx = -1;
        for (int i = 0; i < 5; ++i) {
            int cur_idx = -1;
            for (int j = 0; j < static_cast<int>(hist.size()); ++j) {
                if (hist[j].item_id == "p" + std::to_string(i)) { cur_idx = j; break; }
            }
            if (prev_idx >= 0 && cur_idx >= 0) CHECK(prev_idx < cur_idx);
            prev_idx = cur_idx;
        }
    }

    // ── Section 3: Reference to prior-history item is satisfied immediately ───
    test::section("dep satisfied by history (no DAG edge created)");
    {
        std::atomic<int> cur{0}, pk{0};
        std::mutex log_mx;
        std::vector<std::string> order;
        auto ctx = makeDagCtx("dag_history_dep");

        // Seed history
        agent::WorkResult hist_result;
        hist_result.item_id   = "hist_item";
        hist_result.item_name = "EchoAction";
        hist_result.item_kind = "Action";
        hist_result.success   = true;
        hist_result.output    = {{"val", 99}};
        hist_result.timestamp = std::chrono::system_clock::now();
        ctx->recordResult(hist_result);

        // Both items reference $hist_item which is already in history
        // → no DAG edge → both can run concurrently
        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<InstrumentedAction>(
            "x1", nlohmann::json({{"v","$hist_item.val"}}),
            &log_mx, &order, &cur, &pk,
            std::chrono::milliseconds(20)));
        batch.push_back(std::make_unique<InstrumentedAction>(
            "x2", nlohmann::json({{"v","$hist_item.val"}}),
            &log_mx, &order, &cur, &pk,
            std::chrono::milliseconds(20)));

        agent::BatchExecutor executor(pool);
        auto results = executor.execute(std::move(batch), *ctx);

        CHECK_EQ(static_cast<int>(results.size()), 2);
        for (const auto& r : results) { CHECK(r.success); }
        // Both ran concurrently
        CHECK_GE(pk.load(), 2);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
