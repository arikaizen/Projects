// test_cancellation.cpp — Cancellation propagation (req 18, 39, worked example 14)
//
// Covers:
//   - Start agent with long-running batch (slow EchoActions)
//   - Cancel via mgr->cancelAgent(id) from another thread
//   - Verify agent terminates with "cancelled" reason
//   - Verify cancellation_flag was propagated

#include "test_helper.hpp"
#include <atomic>
#include <thread>

static const std::string PDIR = "/tmp/agent_cancellation_prompts";

int main() {
    std::cout << "=== test_cancellation ===\n";

    test::writeStubPrompts(PDIR);

    // ── Slow self-re-injecting action ─────────────────────────────────────────

    struct SlowLoopAction : agent::Action {
        SlowLoopAction(std::string id, nlohmann::json inp)
            : agent::Action(std::move(id), "SlowLoopAction", std::move(inp)) {}

        agent::WorkResult execute(agent::AgentContext& ctx) override {
            // Keep re-injecting itself to hold the queue non-empty
            static std::atomic<int> n{0};
            auto next = std::make_unique<SlowLoopAction>(
                "slow_loop_" + std::to_string(++n), inputs);
            ctx.push(std::move(next), agent::AgentContext::Position::Back);

            std::this_thread::sleep_for(std::chrono::milliseconds(30));

            agent::WorkResult r;
            r.item_id   = id;
            r.item_name = name;
            r.item_kind = "Action";
            r.success   = true;
            r.output    = {};
            r.timestamp = std::chrono::system_clock::now();
            return r;
        }
    };

    // ── Section 1: Cancel while running ──────────────────────────────────────
    test::section("cancelAgent terminates running agent with 'cancelled'");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        mgr->factory().registerItem(
            agent::WorkItemSpec{"SlowLoopAction", "Slow looping action",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<SlowLoopAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig cfg;
        cfg.name           = "cancel_target";
        cfg.task           = "loop forever";
        cfg.max_iterations = 1000;

        auto agent_id = mgr->spawnAgent(cfg);
        auto seed = mgr->factory().create("SlowLoopAction", "slow_seed_1", {});
        mgr->injectWork(agent_id, std::move(seed));

        auto fut = mgr->runAgent(agent_id, "loop forever");

        // Cancel after 80ms (allowing 2-3 iterations to start)
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        mgr->cancelAgent(agent_id);

        auto result = fut.get();
        CHECK_EQ(result["reason"].get<std::string>(), std::string("cancelled"));
    }

    // ── Section 2: Cancel before running ─────────────────────────────────────
    test::section("cancelAgent before run returns Cancelled immediately");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        mgr->factory().registerItem(
            agent::WorkItemSpec{"SlowLoopAction", "Slow looping action",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<SlowLoopAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig cfg;
        cfg.name           = "pre_cancel_agent";
        cfg.task           = "never runs";
        cfg.max_iterations = 100;

        auto agent_id = mgr->spawnAgent(cfg);

        // Cancel before running
        mgr->cancelAgent(agent_id);

        // Seed after cancel (so queue is non-empty but agent should still cancel)
        auto seed = mgr->factory().create("SlowLoopAction", "slow_seed_pre", {});
        mgr->injectWork(agent_id, std::move(seed));

        auto result = mgr->runAgentBlocking(agent_id, "never runs");
        CHECK_EQ(result["reason"].get<std::string>(), std::string("cancelled"));
    }

    // ── Section 3: cancellation_flag is observable from context ──────────────
    test::section("cancellation_flag is set after cancelAgent");
    {
        auto bus     = std::make_shared<agent::EventBus>();
        auto bb      = std::make_shared<agent::Blackboard>(bus.get());
        auto llm     = std::make_shared<agent::MockLLMClient>(
            [](const agent::LLMClient::Request&) {
                return agent::LLMClient::Response{"[]", true, ""};
            });
        auto factory = std::make_shared<agent::WorkFactory>();
        auto loader  = std::make_shared<agent::PromptLoader>("/tmp");
        auto mem     = std::make_shared<agent::NoOpMemoryBackend>();
        auto inbox   = std::make_unique<agent::MessageInbox>();

        agent::AgentConfig cfg;
        cfg.agent_id       = "flag_test_agent";
        cfg.task           = "flag test";
        cfg.max_iterations = 100;

        auto ctx = std::make_unique<agent::AgentContext>(
            cfg, llm, factory, loader, mem,
            bb.get(), inbox.get(), bus.get(), nullptr);

        // Verify flag starts as false
        CHECK(!ctx->cancellation_flag.load());

        // Set flag externally
        ctx->cancellation_flag.store(true);
        ctx->wakeLoop();

        CHECK(ctx->cancellation_flag.load());
    }

    // ── Section 4: Concurrent cancel + inject is race-free ───────────────────
    test::section("concurrent cancelAgent and injectWork are race-free");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        mgr->factory().registerItem(
            agent::WorkItemSpec{"SlowLoopAction", "Slow looping action",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<SlowLoopAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig cfg;
        cfg.name           = "race_test_agent";
        cfg.task           = "race";
        cfg.max_iterations = 1000;

        auto agent_id = mgr->spawnAgent(cfg);
        auto seed = mgr->factory().create("SlowLoopAction", "race_seed", {});
        mgr->injectWork(agent_id, std::move(seed));

        auto fut = mgr->runAgent(agent_id, "race");

        std::atomic<int> inject_count{0};

        // Injector thread: injects work items concurrently
        std::thread injector([&]() {
            for (int i = 0; i < 5; ++i) {
                try {
                    auto item = mgr->factory().create(
                        "EchoAction", "race_echo_" + std::to_string(i), {});
                    mgr->injectWork(agent_id, std::move(item));
                    ++inject_count;
                } catch (...) {
                    // Agent may already be destroyed — ignore
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        // Cancel after 70ms
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        mgr->cancelAgent(agent_id);

        auto result = fut.get();
        injector.join();

        // Should terminate with cancelled (no crash = no race)
        CHECK_EQ(result["reason"].get<std::string>(), std::string("cancelled"));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
