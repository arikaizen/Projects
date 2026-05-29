// test_determinism.cpp — Deterministic history ordering (req 33)
//
// Verifies that BatchExecutor records results in declared order,
// regardless of which thread finishes first.

#include "test_helper.hpp"
#include "agent/batch_executor.hpp"
#include "agent/thread_pool.hpp"
#include <chrono>
#include <thread>

namespace {

// SlowAction: sleeps for delay_ms before succeeding.
struct SlowAction : agent::Action {
    int delay_ms;
    SlowAction(std::string id_, nlohmann::json inp, int d)
        : agent::Action(std::move(id_), "SlowAction", std::move(inp))
        , delay_ms(d) {}

    agent::WorkResult execute(agent::AgentContext&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = {{"value", id}};
        r.timestamp = std::chrono::system_clock::now();
        r.duration  = std::chrono::milliseconds(delay_ms);
        return r;
    }
};

} // anonymous namespace

int main() {
    std::cout << "=== test_determinism ===\n";

    // ── Section 1: Declared order preserved despite varying completion order ──
    test::section("history order matches declared order");
    {
        auto bus = std::make_shared<agent::EventBus>();
        auto bb  = std::make_shared<agent::Blackboard>(bus.get());
        auto ctx = makeTestContext("det_agent", "det_task", nullptr, bus, bb);
        agent::ThreadPool pool(4);
        agent::BatchExecutor exec(pool);

        // Declare: slow(200ms), fast(10ms), medium(80ms)
        // They will finish in order: fast, medium, slow
        // But history must be: slow, fast, medium (declared order)
        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<SlowAction>("s1", nlohmann::json{}, 200));
        batch.push_back(std::make_unique<SlowAction>("s2", nlohmann::json{}, 10));
        batch.push_back(std::make_unique<SlowAction>("s3", nlohmann::json{}, 80));

        auto results = exec.execute(std::move(batch), *ctx);

        CHECK_EQ(static_cast<int>(results.size()), 3);
        // Declared order must be preserved
        if (results.size() == 3) {
            CHECK_EQ(results[0].item_id, std::string("s1"));
            CHECK_EQ(results[1].item_id, std::string("s2"));
            CHECK_EQ(results[2].item_id, std::string("s3"));
        }
    }

    // ── Section 2: History vector preserves declared order ────────────────────
    test::section("ctx.history() preserves declared order");
    {
        auto bus = std::make_shared<agent::EventBus>();
        auto bb  = std::make_shared<agent::Blackboard>(bus.get());
        auto ctx = makeTestContext("det2", "task", nullptr, bus, bb);
        agent::ThreadPool pool(4);
        agent::BatchExecutor exec(pool);

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        for (int i = 0; i < 6; ++i) {
            int delay = (i % 2 == 0) ? 50 : 5; // alternating slow/fast
            batch.push_back(std::make_unique<SlowAction>(
                "item" + std::to_string(i), nlohmann::json{}, delay));
        }

        exec.execute(std::move(batch), *ctx);

        const auto& hist = ctx->history();
        CHECK_EQ(static_cast<int>(hist.size()), 6);
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            std::string expected = "item" + std::to_string(i);
            CHECK_EQ(hist[i].item_id, expected);
        }
    }

    // ── Section 3: ran_in_parallel flag set for batch > 1 ────────────────────
    test::section("ran_in_parallel flag");
    {
        auto bus = std::make_shared<agent::EventBus>();
        auto bb  = std::make_shared<agent::Blackboard>(bus.get());
        auto ctx = makeTestContext("par_agent", "task", nullptr, bus, bb);
        agent::ThreadPool pool(4);
        agent::BatchExecutor exec(pool);

        std::vector<std::unique_ptr<agent::WorkItem>> batch;
        batch.push_back(std::make_unique<SlowAction>("p1", nlohmann::json{}, 5));
        batch.push_back(std::make_unique<SlowAction>("p2", nlohmann::json{}, 5));

        auto results = exec.execute(std::move(batch), *ctx);
        CHECK_EQ(static_cast<int>(results.size()), 2);
        // Items in a multi-item batch should have ran_in_parallel true
        if (results.size() == 2) {
            CHECK(results[0].ran_in_parallel || results[1].ran_in_parallel);
        }
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
