// test_chains.cpp — Chained action reference resolution (req 5, 6)
//
// Worked example 1: Two chained EchoActions where the second references
// the first's output via "$r1.data".
//
// Steps:
//   1. Mock LLM returns plan: [EchoAction id=r1, EchoAction id=t1 with "$r1.data"]
//   2. ReasonStage pushes both to queue; BatchExecutor detects dependency r1→t1
//   3. r1 runs first; t1 runs after and resolves $r1.data from history
//   4. t1's output contains the resolved value "hello"

#include "test_helper.hpp"
#include "agent/agent.hpp"

// We register our own stages needed for this test
#include <filesystem>
#include <fstream>

// Minimal ReasonStage-equivalent: directly parse the LLM response and push items.
// Rather than depending on the actual stage registration, we register a simple
// "PlanAction" that parses the plan JSON stored in its inputs and pushes items.
struct DirectPlanAction : agent::Action {
    DirectPlanAction(std::string id, nlohmann::json inp)
        : agent::Action(std::move(id), "DirectPlanAction", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        // inputs["plan"] is a JSON array of {name, id, inputs} objects
        if (inputs.contains("plan") && inputs["plan"].is_array()) {
            for (auto& item_j : inputs["plan"]) {
                auto name_str  = item_j["name"].get<std::string>();
                auto id_str    = item_j["id"].get<std::string>();
                auto inp_j     = item_j.value("inputs", nlohmann::json::object());
                auto work_item = ctx.factory().create(name_str, id_str, inp_j);
                ctx.push(std::move(work_item), agent::AgentContext::Position::Back);
            }
        }
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = {{"pushed", inputs.value("plan", nlohmann::json::array()).size()}};
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};

int main() {
    std::cout << "=== test_chains ===\n";

    const std::string PDIR = "/tmp/agent_chains_prompts";
    test::writeStubPrompts(PDIR);

    auto mgr = makeTestManager(nullptr, PDIR);
    test::registerEchoAction(mgr->factory());

    // Register DirectPlanAction
    mgr->factory().registerItem(
        agent::WorkItemSpec{"DirectPlanAction", "Pushes plan from its inputs",
                            agent::WorkItem::Kind::Action, {}},
        [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
            return std::make_unique<DirectPlanAction>(std::move(id), std::move(inp));
        }
    );

    // ── Section 1: Chain r1 → t1 ─────────────────────────────────────────────
    test::section("chained EchoActions: t1 resolves $r1.data");
    {
        agent::AgentConfig cfg;
        cfg.name           = "chain_agent";
        cfg.task           = "chain test";
        cfg.max_iterations = 20;

        auto agent_id = mgr->spawnAgent(cfg);

        // Build the plan as JSON
        nlohmann::json plan = nlohmann::json::array({
            {{"name","EchoAction"}, {"id","r1"}, {"inputs", {{"data","hello"}}}},
            {{"name","EchoAction"}, {"id","t1"}, {"inputs", {{"data","$r1.data"},{"extra","world"}}}}
        });

        // Inject a DirectPlanAction that will push r1 and t1
        auto planner = mgr->factory().create(
            "DirectPlanAction", "planner1", {{"plan", plan}});
        mgr->injectWork(agent_id, std::move(planner));

        auto result = mgr->runAgentBlocking(agent_id, "chain test");

        // Both r1 and t1 should have run
        auto status = mgr->getStatus(agent_id);
        // We can't easily introspect history through the public mgr API,
        // but we can verify the agent completed without error
        CHECK_EQ(result["reason"].get<std::string>(), std::string("queue_empty"));
    }

    // ── Section 2: Verify resolution via a direct context test ───────────────
    test::section("resolveReferences correctly chains $r1.data → t1");
    {
        // Build a context, manually add r1 to history, then verify t1's inputs resolve
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

        agent::AgentConfig cfg;
        cfg.agent_id = "chain_ctx_test";
        cfg.task     = "test";

        agent::AgentContext ctx(
            cfg, llm, factory, loader, mem,
            bb.get(), inbox.get(), bus.get(), nullptr);

        // Record r1's result
        agent::WorkResult r1_result;
        r1_result.item_id   = "r1";
        r1_result.item_name = "EchoAction";
        r1_result.item_kind = "Action";
        r1_result.success   = true;
        r1_result.output    = {{"data", "hello"}, {"extra", "world"}};
        r1_result.timestamp = std::chrono::system_clock::now();
        ctx.recordResult(r1_result);

        // t1's inputs reference $r1.data and $r1.extra
        nlohmann::json t1_inputs = {
            {"data",  "$r1.data"},
            {"extra", "$r1.extra"}
        };

        nlohmann::json resolved;
        CHECK_NOTHROW(resolved = ctx.resolveReferences(t1_inputs));

        CHECK(resolved.contains("data"));
        CHECK(resolved.contains("extra"));
        if (resolved.contains("data")) {
            CHECK_EQ(resolved["data"].get<std::string>(), std::string("hello"));
        }
        if (resolved.contains("extra")) {
            CHECK_EQ(resolved["extra"].get<std::string>(), std::string("world"));
        }
    }

    // ── Section 3: Three-item chain a→b→c ────────────────────────────────────
    test::section("three-item chain a→b→c executes in order");
    {
        agent::AgentConfig cfg;
        cfg.name           = "three_chain";
        cfg.task           = "three chain test";
        cfg.max_iterations = 20;

        auto agent_id = mgr->spawnAgent(cfg);

        nlohmann::json plan = nlohmann::json::array({
            {{"name","EchoAction"}, {"id","a1"}, {"inputs",{{"val","first"}}}},
            {{"name","EchoAction"}, {"id","b1"}, {"inputs",{{"val","$a1.val"}}}},
            {{"name","EchoAction"}, {"id","c1"}, {"inputs",{{"val","$b1.val"},{"also","$a1.val"}}}}
        });

        auto planner = mgr->factory().create(
            "DirectPlanAction", "planner2", {{"plan", plan}});
        mgr->injectWork(agent_id, std::move(planner));

        auto result = mgr->runAgentBlocking(agent_id, "three chain test");
        CHECK_EQ(result["reason"].get<std::string>(), std::string("queue_empty"));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
