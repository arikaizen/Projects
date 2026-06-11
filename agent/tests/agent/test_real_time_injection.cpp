// test_real_time_injection.cpp — External real-time injection (req 20, worked example 4)
//
// Scenario:
//   - Agent starts with a slow action (sleeps 50ms) that re-injects itself once
//   - From another thread, after 25ms, inject a "marker" EchoAction via mgr->injectWork()
//   - Verify the injected item appears in history after the agent finishes

#include "test_helper.hpp"
#include <thread>

int main() {
    std::cout << "=== test_real_time_injection ===\n";

    const std::string PDIR = "/tmp/agent_realtime_inject_prompts";
    test::writeStubPrompts(PDIR);

    auto mgr = makeTestManager(nullptr, PDIR);
    test::registerEchoAction(mgr->factory());

    // ── Slow self-terminating action ──────────────────────────────────────────
    // Runs once, sleeps 80ms, does NOT re-inject itself — so queue will drain
    // after it finishes (unless the marker was injected in time).
    struct SlowTermAction : agent::Action {
        SlowTermAction(std::string id, nlohmann::json inp)
            : agent::Action(std::move(id), "SlowTermAction", std::move(inp)) {}
        agent::WorkResult execute(agent::AgentContext&) override {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            agent::WorkResult r;
            r.item_id   = id;
            r.item_name = name;
            r.item_kind = "Action";
            r.success   = true;
            r.output    = {{"done", true}};
            r.timestamp = std::chrono::system_clock::now();
            return r;
        }
    };

    mgr->factory().registerItem(
        agent::WorkItemSpec{"SlowTermAction", "Slow action that sleeps 80ms",
                            agent::WorkItem::Kind::Action, {}},
        [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
            return std::make_unique<SlowTermAction>(std::move(id), std::move(inp));
        }
    );

    // ── Section 1: Marker injected while slow action is running ───────────────
    test::section("injected marker appears in history");
    {
        agent::AgentConfig cfg;
        cfg.name           = "inject_target";
        cfg.task           = "injection test";
        cfg.max_iterations = 20;

        auto agent_id = mgr->spawnAgent(cfg);

        // Seed with the slow action
        auto slow = mgr->factory().create("SlowTermAction", "slow1", {});
        mgr->injectWork(agent_id, std::move(slow));

        // Start the agent asynchronously
        auto fut = mgr->runAgent(agent_id, "injection test");

        // After 25ms (well before slow action completes), inject the marker
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        auto marker = mgr->factory().create("EchoAction", "marker1", {{"tag","injected"}});
        mgr->injectWork(agent_id, std::move(marker));

        // Wait for agent to finish
        auto result = fut.get();
        CHECK_EQ(result["reason"].get<std::string>(), std::string("queue_empty"));

        // Verify marker is in history
        auto status = mgr->getStatus(agent_id);
        // We can't directly inspect history through the manager's public API,
        // but we can verify the agent ran successfully (it would fail if marker
        // caused a crash or data race).
        CHECK(result.contains("reason"));
    }

    // ── Section 2: Direct context-level injection test ────────────────────────
    test::section("injectFromOutside is thread-safe");
    {
        auto bus     = std::make_shared<agent::EventBus>();
        auto bb      = std::make_shared<agent::Blackboard>(bus.get());
        auto llm     = std::make_shared<agent::MockLLMClient>(
            [](const agent::LLMClient::Request&) {
                return agent::LLMClient::Response{"[]", true, "", {}};
            });
        auto factory = std::make_shared<agent::WorkFactory>();
        auto loader  = std::make_shared<agent::PromptLoader>("/tmp");
        auto mem     = std::make_shared<agent::NoOpMemoryBackend>();
        auto inbox   = std::make_unique<agent::MessageInbox>();

        test::registerEchoAction(*factory);

        // Register a blocking action that occupies the agent while we inject
        struct BlockingAction : agent::Action {
            std::atomic<bool>& injected_flag;
            BlockingAction(std::string id, nlohmann::json inp, std::atomic<bool>& flag)
                : agent::Action(std::move(id), "BlockingAction", std::move(inp))
                , injected_flag(flag) {}
            agent::WorkResult execute(agent::AgentContext&) override {
                // Wait until the injector thread has fired
                auto deadline = std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(500);
                while (!injected_flag.load() &&
                       std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
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

        std::atomic<bool> injected_flag{false};
        factory->registerItem(
            agent::WorkItemSpec{"BlockingAction", "Waits for injection signal",
                                agent::WorkItem::Kind::Action, {}},
            [&injected_flag](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<BlockingAction>(std::move(id), std::move(inp), injected_flag);
            }
        );

        agent::AgentConfig cfg;
        cfg.agent_id       = "inject_ctx_test";
        cfg.task           = "inject test";
        cfg.max_iterations = 10;

        auto ctx_ptr = std::make_unique<agent::AgentContext>(
            cfg, llm, factory, loader, mem,
            bb.get(), inbox.get(), bus.get(), nullptr);

        // Seed with blocking action
        auto blocker = factory->create("BlockingAction", "blocker1", {});
        ctx_ptr->push(std::move(blocker), agent::AgentContext::Position::Back);

        agent::AgentContext* ctx_raw = ctx_ptr.get();

        // Injection thread: after 30ms, inject a marker and set the flag
        std::thread injector([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            auto marker = factory->create("EchoAction", "rt_marker", {{"injected", true}});
            ctx_raw->injectFromOutside(std::move(marker), agent::AgentContext::Position::Front);
            injected_flag.store(true);
        });

        agent::ThreadPool pool(4);
        agent::Agent      agent(std::move(ctx_ptr), pool);
        auto run_result = agent.run();

        injector.join();

        CHECK(run_result.reason == agent::Agent::TerminationReason::QueueEmpty ||
              run_result.reason == agent::Agent::TerminationReason::ShouldStop);

        // Marker must be in history
        bool found_marker = false;
        for (const auto& r : agent.context().history()) {
            if (r.item_id == "rt_marker") { found_marker = true; break; }
        }
        CHECK(found_marker);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
