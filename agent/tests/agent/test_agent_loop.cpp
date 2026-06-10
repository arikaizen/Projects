// test_agent_loop.cpp — Agent loop termination condition tests (req 16)
//
// Covers:
//   - Empty queue → QueueEmpty termination
//   - should_stop set externally → ShouldStop termination
//   - max_iterations exceeded → MaxIterations termination
//   - cancellation_flag set → Cancelled termination

#include "test_helper.hpp"
#include "agent/agent.hpp"

// ── Helper: build a manager with EchoAction registered and a fresh agent ─────

static std::shared_ptr<agent::AgentManager> makeLoopManager(
    agent::LLMClient::Handler handler = nullptr,
    const std::string& prompts_dir    = "/tmp/agent_loop_test_prompts")
{
    auto mgr = makeTestManager(handler, prompts_dir);
    test::registerEchoAction(mgr->factory());
    return mgr;
}

int main() {
    std::cout << "=== test_agent_loop ===\n";

    // ── Section 1: Empty queue → QueueEmpty ───────────────────────────────────
    test::section("empty queue terminates with QueueEmpty");
    {
        // LLM returns empty plan → no items pushed → queue drains immediately
        auto mgr = makeLoopManager(
            [](const agent::LLMClient::Request&) {
                return agent::LLMClient::Response{"[]", true, "", {}};
            },
            "/tmp/agent_loop_empty");

        agent::AgentConfig cfg;
        cfg.name           = "empty_queue_agent";
        cfg.task           = "do nothing";
        cfg.max_iterations = 10;

        auto id = mgr->spawnAgent(cfg);
        // No items injected → queue is empty from the start
        auto result = mgr->runAgentBlocking(id, "do nothing");

        CHECK_EQ(result["reason"].get<std::string>(), std::string("queue_empty"));
    }

    // ── Section 2: should_stop flag → ShouldStop ──────────────────────────────
    test::section("should_stop flag terminates with ShouldStop");
    {
        // The LLM returns a final_answer top-level object which causes ReasonStage
        // to set should_stop = true.  We need ReasonStage registered for this path.
        // Simpler approach: inject an EchoAction that sets should_stop via a
        // custom Action subclass.
        struct StopAction : agent::Action {
            StopAction(std::string id, nlohmann::json inp)
                : agent::Action(std::move(id), "StopAction", std::move(inp)) {}
            agent::WorkResult execute(agent::AgentContext& ctx) override {
                ctx.should_stop  = true;
                ctx.final_output = {{"answer", "done"}};
                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = true;
                r.output    = {{"stopped", true}};
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        auto mgr = makeLoopManager(nullptr, "/tmp/agent_loop_stop");
        mgr->factory().registerItem(
            agent::WorkItemSpec{"StopAction", "Sets should_stop",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<StopAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig cfg;
        cfg.name           = "stop_agent";
        cfg.task           = "stop";
        cfg.max_iterations = 50;

        auto id = mgr->spawnAgent(cfg);

        // Inject a StopAction so the agent runs it and sets should_stop
        auto stop_item = mgr->factory().create("StopAction", "stopper1", {});
        mgr->injectWork(id, std::move(stop_item));

        auto result = mgr->runAgentBlocking(id, "stop");
        CHECK_EQ(result["reason"].get<std::string>(), std::string("should_stop"));
    }

    // ── Section 3: max_iterations exceeded → MaxIterations ───────────────────
    test::section("max_iterations exceeded terminates with MaxIterations");
    {
        // Use an LLM that always returns one EchoAction — agent loops forever
        // unless max_iterations stops it.
        int call_count = 0;
        auto handler = [&call_count](const agent::LLMClient::Request&)
            -> agent::LLMClient::Response {
            ++call_count;
            // Return a plan with one EchoAction per call
            std::string id = "echo_iter_" + std::to_string(call_count);
            nlohmann::json plan = nlohmann::json::array({
                {{"name","EchoAction"},{"id",id},{"inputs",{{"data","loop"}}}}
            });
            return {plan.dump(), true, "", {}};
        };

        auto mgr = makeLoopManager(handler, "/tmp/agent_loop_maxiter");

        agent::AgentConfig cfg;
        cfg.name           = "maxiter_agent";
        cfg.task           = "loop";
        cfg.max_iterations = 3;

        // We also need ReasonStage registered to use the LLM
        // For simplicity: inject EchoActions directly so the agent loops
        // without needing the LLM path through ReasonStage.
        // We use an Action that re-injects itself a limited number of times.
        struct LoopAction : agent::Action {
            LoopAction(std::string id, nlohmann::json inp)
                : agent::Action(std::move(id), "LoopAction", std::move(inp)) {}
            agent::WorkResult execute(agent::AgentContext& ctx) override {
                // Inject another copy of itself so the queue is never empty
                static std::atomic<int> counter{0};
                int n = ++counter;
                auto next = std::make_unique<LoopAction>(
                    "loop_" + std::to_string(n), inputs);
                ctx.push(std::move(next), agent::AgentContext::Position::Back);

                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = true;
                r.output    = {{"iter", n}};
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        mgr->factory().registerItem(
            agent::WorkItemSpec{"LoopAction", "Re-injects itself",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<LoopAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig loop_cfg;
        loop_cfg.name           = "looper";
        loop_cfg.task           = "loop";
        loop_cfg.max_iterations = 3;

        auto agent_id = mgr->spawnAgent(loop_cfg);
        auto seed     = mgr->factory().create("LoopAction", "loop_seed", {});
        mgr->injectWork(agent_id, std::move(seed));

        auto result = mgr->runAgentBlocking(agent_id, "loop");
        CHECK_EQ(result["reason"].get<std::string>(), std::string("max_iterations"));
        CHECK_GE(result["iterations"].get<int>(), 3);
    }

    // ── Section 4: cancellation_flag → Cancelled ──────────────────────────────
    test::section("cancellation_flag terminates with Cancelled");
    {
        // Use a slow EchoAction that sleeps so cancellation fires mid-run
        struct SlowAction : agent::Action {
            SlowAction(std::string id, nlohmann::json inp)
                : agent::Action(std::move(id), "SlowAction", std::move(inp)) {}
            agent::WorkResult execute(agent::AgentContext& ctx) override {
                // Re-inject self to keep queue non-empty
                static std::atomic<int> n{0};
                auto next = std::make_unique<SlowAction>(
                    "slow_" + std::to_string(++n), inputs);
                ctx.push(std::move(next), agent::AgentContext::Position::Back);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

        auto mgr = makeLoopManager(nullptr, "/tmp/agent_loop_cancel");
        mgr->factory().registerItem(
            agent::WorkItemSpec{"SlowAction", "Slow self-re-injecting action",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<SlowAction>(std::move(id), std::move(inp));
            }
        );

        agent::AgentConfig cfg;
        cfg.name           = "cancel_target";
        cfg.task           = "run";
        cfg.max_iterations = 1000;

        auto agent_id = mgr->spawnAgent(cfg);

        // Seed the queue
        auto seed = mgr->factory().create("SlowAction", "slow_seed", {});
        mgr->injectWork(agent_id, std::move(seed));

        // Run async, cancel from another thread after a short delay
        auto fut = mgr->runAgent(agent_id, "run");

        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        mgr->cancelAgent(agent_id);

        auto result = fut.get();
        CHECK_EQ(result["reason"].get<std::string>(), std::string("cancelled"));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
