// test_injection_reading2.cpp — Reading 2: InjectionStage as meta-stage (req 3)
//
// Setup:
//   Queue: [EchoAction(id=e1), InjectionStage]
//
//   EchoAction produces output {"result": "3 errors found"}
//   InjectionStage's mock LLM sees the previous result and returns
//     a plan with 3 EchoActions injected at the FRONT.
//
// Verification:
//   - 3 EchoActions were pushed to front and executed
//   - History contains: e1, injection stage, then the 3 injected actions

#include "test_helper.hpp"
#include "agent/agent.hpp"
#include "agent/action.hpp"
#include "agent/stage.hpp"

// ── MockInjectionStage ────────────────────────────────────────────────────────
// A simplified version that reads the last result and calls the LLM.
// The LLM returns items that get pushed to the FRONT of the queue.

struct MockInjectionStage : agent::Stage {
    MockInjectionStage(std::string id, nlohmann::json inp)
        : agent::Stage(std::move(id), "MockInjectionStage", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        auto start = std::chrono::steady_clock::now();
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Stage";
        r.timestamp = std::chrono::system_clock::now();

        const agent::WorkResult* prev = ctx.lastResult();
        if (!prev) {
            r.success = false;
            r.error   = "MockInjectionStage: no previous result";
            r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            return r;
        }

        // Call LLM with the previous result in context
        agent::LLMClient::Request req;
        req.system_prompt = "injection: " + prev->output.dump();
        req.user_message  = "what to inject?";
        req.json_mode     = true;

        auto resp = ctx.llm().complete(req);
        if (!resp.success) {
            r.success = false;
            r.error   = resp.error;
            r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            return r;
        }

        nlohmann::json plan;
        try {
            plan = nlohmann::json::parse(resp.content);
        } catch (...) {
            r.success = false;
            r.error   = "invalid JSON from LLM";
            r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            return r;
        }

        // Push items to FRONT in reverse order to preserve plan ordering
        std::vector<std::unique_ptr<agent::WorkItem>> items;
        for (const auto& item_j : plan) {
            if (!item_j.is_object()) continue;
            auto name_s = item_j["name"].get<std::string>();
            auto id_s   = item_j["id"].get<std::string>();
            auto inp_j  = item_j.value("inputs", nlohmann::json::object());
            items.push_back(ctx.factory().create(name_s, id_s, inp_j));
        }
        // Push in reverse so that first item ends up at front
        for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) {
            ctx.push(std::move(items[i]), agent::AgentContext::Position::Front);
        }

        r.success = true;
        r.output  = {{"injected_count", plan.size()}, {"previous_item", prev->item_id}};
        r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        return r;
    }
};

int main() {
    std::cout << "=== test_injection_reading2 ===\n";

    const std::string PDIR = "/tmp/agent_reading2_prompts";
    test::writeStubPrompts(PDIR);

    // Call counter: first call is for the e1 EchoAction (but it has no LLM call).
    // The LLM is only called by InjectionStage.
    auto handler = [](const agent::LLMClient::Request& req) -> agent::LLMClient::Response {
        // Return 3 EchoActions when InjectionStage asks
        nlohmann::json plan = nlohmann::json::array({
            {{"name","EchoAction"},{"id","inj1"},{"inputs",{{"data","error1"}}}},
            {{"name","EchoAction"},{"id","inj2"},{"inputs",{{"data","error2"}}}},
            {{"name","EchoAction"},{"id","inj3"},{"inputs",{{"data","error3"}}}}
        });
        return {plan.dump(), true, ""};
    };

    // Build components directly to access history
    auto bus     = std::make_shared<agent::EventBus>();
    auto bb      = std::make_shared<agent::Blackboard>(bus.get());
    auto llm     = std::make_shared<agent::MockLLMClient>(handler);
    auto factory = std::make_shared<agent::WorkFactory>();
    auto loader  = std::make_shared<agent::PromptLoader>("/tmp");
    auto mem     = std::make_shared<agent::NoOpMemoryBackend>();
    auto inbox   = std::make_unique<agent::MessageInbox>();

    test::registerEchoAction(*factory);
    factory->registerItem(
        agent::WorkItemSpec{"MockInjectionStage", "Mock injection stage",
                            agent::WorkItem::Kind::Stage, {}},
        [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
            return std::make_unique<MockInjectionStage>(std::move(id), std::move(inp));
        }
    );

    agent::AgentConfig cfg;
    cfg.agent_id       = "reading2_agent";
    cfg.task           = "test reading 2";
    cfg.max_iterations = 20;

    auto ctx_ptr = std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);

    // ── Section 1: Queue EchoAction then InjectionStage ──────────────────────
    test::section("initial queue: e1 → InjectionStage");
    {
        // Push e1 first
        auto e1 = factory->create("EchoAction", "e1", {{"result","3 errors found"}});
        ctx_ptr->push(std::move(e1), agent::AgentContext::Position::Back);

        // Push InjectionStage after
        auto inj = factory->create("MockInjectionStage", "inj_stage", {});
        ctx_ptr->push(std::move(inj), agent::AgentContext::Position::Back);

        CHECK_EQ(static_cast<int>(ctx_ptr->queueSize()), 2);
    }

    agent::ThreadPool pool(4);
    agent::Agent      agent(std::move(ctx_ptr), pool);

    // ── Section 2: Run and verify history ────────────────────────────────────
    test::section("3 injected EchoActions run and appear in history");
    {
        auto run_result = agent.run();

        // Agent terminates normally
        CHECK(run_result.reason == agent::Agent::TerminationReason::QueueEmpty ||
              run_result.reason == agent::Agent::TerminationReason::ShouldStop);

        const auto& hist = agent.context().history();

        // Expected: e1, inj_stage, inj1, inj2, inj3 — at minimum 5 entries
        CHECK_GE(static_cast<int>(hist.size()), 5);

        bool found_e1 = false, found_is = false;
        bool found_i1 = false, found_i2 = false, found_i3 = false;
        for (const auto& r : hist) {
            if (r.item_id == "e1")        found_e1 = true;
            if (r.item_id == "inj_stage") found_is = true;
            if (r.item_id == "inj1")      found_i1 = true;
            if (r.item_id == "inj2")      found_i2 = true;
            if (r.item_id == "inj3")      found_i3 = true;
        }
        CHECK(found_e1);
        CHECK(found_is);
        CHECK(found_i1);
        CHECK(found_i2);
        CHECK(found_i3);

        // Verify ordering: e1 before injection stage, injection stage before inj1-3
        int idx_e1 = -1, idx_is = -1, idx_i1 = -1, idx_i2 = -1, idx_i3 = -1;
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            if (hist[i].item_id == "e1")        idx_e1 = i;
            if (hist[i].item_id == "inj_stage") idx_is = i;
            if (hist[i].item_id == "inj1")      idx_i1 = i;
            if (hist[i].item_id == "inj2")      idx_i2 = i;
            if (hist[i].item_id == "inj3")      idx_i3 = i;
        }

        if (idx_e1 >= 0 && idx_is >= 0) CHECK(idx_e1 < idx_is);
        if (idx_is >= 0 && idx_i1 >= 0) CHECK(idx_is < idx_i1);
        if (idx_i1 >= 0 && idx_i2 >= 0) CHECK(idx_i1 <= idx_i2);  // declared order
        if (idx_i2 >= 0 && idx_i3 >= 0) CHECK(idx_i2 <= idx_i3);
    }

    // ── Section 3: InjectionStage's output records what it injected ──────────
    test::section("InjectionStage output records injected_count");
    {
        const auto& hist = agent.context().history();
        const agent::WorkResult* inj_result = nullptr;
        for (const auto& r : hist) {
            if (r.item_id == "inj_stage") { inj_result = &r; break; }
        }
        CHECK(inj_result != nullptr);
        if (inj_result) {
            CHECK(inj_result->success);
            CHECK(inj_result->output.contains("injected_count"));
            if (inj_result->output.contains("injected_count")) {
                CHECK_EQ(inj_result->output["injected_count"].get<int>(), 3);
            }
        }
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
