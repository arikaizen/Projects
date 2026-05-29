// test_concurrency.cpp — Multi-tenant concurrent run (req 38, worked example 12)
//
// Covers:
//   - M=8 agents across U=2 users running simultaneously
//   - Each agent runs 3 EchoActions and terminates
//   - All M agents produce results
//   - No data races (atomic counters for shared state)
//   - Over-quota spawn returns error (quota max_agents=2, spawn 3rd → throws)

#include "test_helper.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <future>

// ── Unique prompts dir for this test ─────────────────────────────────────────
static const std::string PDIR = "/tmp/agent_concurrency_test_prompts";

int main() {
    std::cout << "=== test_concurrency ===\n";

    test::writeStubPrompts(PDIR);

    // ── Section 1: M=8 agents, each running 3 EchoActions ────────────────────
    test::section("M=8 agents, 3 EchoActions each, all complete");
    {
        const int M       = 8;
        const int U       = 2;  // simulated users
        const int ACTIONS = 3;

        std::atomic<int> completed_agents{0};

        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        // Build N actions plan as a lambda-triggered DirectPlanAction
        // (reuse the pattern from test_chains)
        struct FixedPlanAction : agent::Action {
            int n_actions;
            std::string agent_label;
            FixedPlanAction(std::string id, nlohmann::json inp,
                            int n, std::string label)
                : agent::Action(std::move(id), "FixedPlanAction", std::move(inp))
                , n_actions(n), agent_label(std::move(label)) {}

            agent::WorkResult execute(agent::AgentContext& ctx) override {
                for (int i = 0; i < n_actions; ++i) {
                    auto item = std::make_unique<test::EchoAction>(
                        agent_label + "_echo_" + std::to_string(i),
                        nlohmann::json({{"agent", agent_label}, {"step", i}}));
                    ctx.push(std::move(item), agent::AgentContext::Position::Back);
                }
                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = true;
                r.output    = {{"pushed", n_actions}};
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        mgr->factory().registerItem(
            agent::WorkItemSpec{"FixedPlanAction", "Pushes N EchoActions",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                // Extract n and label from inputs
                int n      = inp.value("n", 3);
                std::string lbl = inp.value("label", id);
                return std::make_unique<FixedPlanAction>(
                    std::move(id), std::move(inp), n, std::move(lbl));
            }
        );

        // Spawn M agents across U users
        std::vector<std::string> agent_ids;
        for (int i = 0; i < M; ++i) {
            agent::AgentConfig cfg;
            cfg.name           = "concurrent_agent_" + std::to_string(i);
            cfg.task           = "run " + std::to_string(ACTIONS) + " actions";
            cfg.max_iterations = 20;
            cfg.extra["user_id"] = "user_" + std::to_string(i % U);

            auto id = mgr->spawnAgent(cfg);
            agent_ids.push_back(id);
        }

        CHECK_EQ(static_cast<int>(agent_ids.size()), M);

        // Seed each agent with a FixedPlanAction, then run them all
        std::vector<std::future<nlohmann::json>> futures;
        for (int i = 0; i < M; ++i) {
            const auto& aid = agent_ids[i];
            std::string label = "ag" + std::to_string(i);
            auto seed = mgr->factory().create(
                "FixedPlanAction", "seed_" + label,
                {{"n", ACTIONS}, {"label", label}});
            mgr->injectWork(aid, std::move(seed));
            futures.push_back(mgr->runAgent(aid, "run"));
        }

        // Wait for all
        for (auto& f : futures) {
            auto result = f.get();
            CHECK_EQ(result["reason"].get<std::string>(), std::string("queue_empty"));
            ++completed_agents;
        }

        CHECK_EQ(completed_agents.load(), M);
    }

    // ── Section 2: Over-quota spawn throws ───────────────────────────────────
    test::section("over-quota spawn throws");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        // Set quota: max 2 agents for this user
        agent::UserQuota q;
        q.max_concurrent_agents = 2;
        mgr->setUserQuota("quota_user", q);

        agent::AgentConfig cfg;
        cfg.name   = "quota_agent";
        cfg.task   = "test";
        cfg.extra["user_id"] = "quota_user";

        std::string id1, id2;
        CHECK_NOTHROW(id1 = mgr->spawnAgent(cfg));
        CHECK_NOTHROW(id2 = mgr->spawnAgent(cfg));

        // Third spawn must fail
        CHECK_THROW(mgr->spawnAgent(cfg));

        // After destroying one, spawn succeeds again
        mgr->destroyAgent(id1);
        std::string id3;
        CHECK_NOTHROW(id3 = mgr->spawnAgent(cfg));

        // Cleanup
        mgr->destroyAgent(id2);
        mgr->destroyAgent(id3);
    }

    // ── Section 3: Concurrent blackboard writes from multiple agents ──────────
    test::section("concurrent blackboard writes are thread-safe");
    {
        const int N = 10;
        std::atomic<int> write_count{0};

        auto mgr = makeTestManager(nullptr, PDIR);

        struct BBWriteAction : agent::Action {
            std::string key;
            int         value;
            std::atomic<int>& counter;
            BBWriteAction(std::string id, nlohmann::json inp, std::string k, int v, std::atomic<int>& c)
                : agent::Action(std::move(id), "BBWriteAction", std::move(inp))
                , key(std::move(k)), value(v), counter(c) {}
            agent::WorkResult execute(agent::AgentContext& ctx) override {
                ctx.blackboard()->write(key, nlohmann::json(value));
                ++counter;
                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = true;
                r.output    = {{"key", key}, {"value", value}};
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        mgr->factory().registerItem(
            agent::WorkItemSpec{"BBWriteAction", "Write to blackboard",
                                agent::WorkItem::Kind::Action, {}},
            [&write_count](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                std::string key = inp.value("key", "default");
                int val         = inp.value("value", 0);
                return std::make_unique<BBWriteAction>(
                    std::move(id), std::move(inp), std::move(key), val, write_count);
            }
        );

        std::vector<std::future<nlohmann::json>> futures;
        for (int i = 0; i < N; ++i) {
            agent::AgentConfig cfg;
            cfg.name           = "bb_agent_" + std::to_string(i);
            cfg.task           = "write";
            cfg.max_iterations = 5;

            auto aid = mgr->spawnAgent(cfg);
            auto item = mgr->factory().create(
                "BBWriteAction", "bbw_" + std::to_string(i),
                {{"key","slot_" + std::to_string(i)}, {"value", i}});
            mgr->injectWork(aid, std::move(item));
            futures.push_back(mgr->runAgent(aid, "write"));
        }

        for (auto& f : futures) f.get();

        // All N writes must have happened
        CHECK_EQ(write_count.load(), N);

        // All keys must be present
        auto keys = mgr->blackboardKeys("slot_");
        CHECK_EQ(static_cast<int>(keys.size()), N);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
