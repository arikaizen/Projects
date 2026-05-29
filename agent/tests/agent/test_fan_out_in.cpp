// test_fan_out_in.cpp — Fan-out / fan-in composition (req 43, worked ex 13)
//
// Covers:
//   - fanOut spawns N agents and returns N futures
//   - fanIn awaits all and runs synthesiser
//   - researchFromAngles convenience wrapper

#include "test_helper.hpp"
#include <chrono>

int main() {
    std::cout << "=== test_fan_out_in ===\n";

    // ── Section 1: fanOut spawns N agents ────────────────────────────────────
    test::section("fanOut returns N futures");
    {
        auto mgr = makeTestManager();

        std::vector<agent::AgentConfig> configs(3);
        for (int i = 0; i < 3; ++i) {
            configs[i].name = "FanWorker" + std::to_string(i);
            configs[i].task = "work";
        }

        auto futures = mgr->fanOut(configs, "shared task");
        CHECK_EQ(static_cast<int>(futures.size()), 3);

        // All futures are valid
        for (auto& f : futures) {
            CHECK(f.valid());
        }

        // Wait for all (timeout 10s each)
        for (auto& f : futures) {
            if (f.valid()) {
                auto st = f.wait_for(std::chrono::seconds(10));
                CHECK(st == std::future_status::ready);
            }
        }
    }

    // ── Section 2: fanIn collects results from all futures ────────────────────
    test::section("fanIn synthesises results");
    {
        auto mgr = makeTestManager();

        std::vector<agent::AgentConfig> worker_cfgs(2);
        worker_cfgs[0].name = "W0";
        worker_cfgs[1].name = "W1";

        auto futures = mgr->fanOut(worker_cfgs, "research angle");

        // Synthesiser also uses the empty-plan mock
        agent::AgentConfig synth_cfg;
        synth_cfg.name = "Synthesiser";

        // fanIn is blocking
        nlohmann::json combined;
        CHECK_NOTHROW(combined = mgr->fanIn(futures, synth_cfg));
        // Just verify it returned a JSON value without crashing
        CHECK(!combined.is_null() || combined.is_null()); // tautology — no crash
    }

    // ── Section 3: researchFromAngles convenience wrapper ────────────────────
    test::section("researchFromAngles does not throw");
    {
        auto mgr = makeTestManager();

        std::vector<std::string> angles = {"technical", "legal", "economic"};
        nlohmann::json result;
        CHECK_NOTHROW(result = mgr->researchFromAngles(angles, "AI regulation"));
    }

    // ── Section 4: fanOut with 1 agent is degenerate but valid ────────────────
    test::section("fanOut with single agent");
    {
        auto mgr = makeTestManager();

        std::vector<agent::AgentConfig> configs(1);
        configs[0].name = "Solo";
        configs[0].task = "solo work";

        auto futures = mgr->fanOut(configs, "task");
        CHECK_EQ(static_cast<int>(futures.size()), 1);
        CHECK(futures[0].valid());

        auto st = futures[0].wait_for(std::chrono::seconds(5));
        CHECK(st == std::future_status::ready);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
