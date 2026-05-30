// test_pattern_c_blackboard.cpp — Pattern C: shared blackboard (req 46-48, worked example 7)
//
// Covers:
//   - Blackboard write / read (returns std::optional) / remove / keys
//   - Three agents writing to separate keys; synthesiser reads all three
//   - AgentManager blackboard* convenience wrappers
//   - Thread-safe concurrent writes

#include "test_helper.hpp"
#include <thread>
#include <vector>

static const std::string PDIR_C = "/tmp/agent_pattern_c_prompts";

int main() {
    std::cout << "=== test_pattern_c_blackboard ===\n";

    test::writeStubPrompts(PDIR_C);

    // ── Section 1: Blackboard direct unit tests ───────────────────────────────
    test::section("Blackboard write and read (optional)");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        CHECK_NOTHROW(bb.write("key1", nlohmann::json{{"v", 42}}));
        // read() returns std::optional<nlohmann::json>
        auto opt = bb.read("key1");
        CHECK(opt.has_value());
        if (opt) {
            CHECK(opt->contains("v"));
            CHECK_EQ((*opt)["v"].get<int>(), 42);
        }
    }

    // ── Section 2: Read missing key returns empty optional ────────────────────
    test::section("read missing key returns empty optional");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        auto opt = bb.read("nonexistent");
        // Returns std::nullopt for missing keys
        CHECK(!opt.has_value());
    }

    // ── Section 3: keys() with prefix filter ─────────────────────────────────
    test::section("keys prefix filter");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        bb.write("ns:alpha", "A");
        bb.write("ns:beta",  "B");
        bb.write("other:gamma", "C");

        auto ns_keys = bb.keys("ns:");
        CHECK_GE(static_cast<int>(ns_keys.size()), 2);
        bool found_alpha = false, found_beta = false;
        for (auto& k : ns_keys) {
            if (k == "ns:alpha") found_alpha = true;
            if (k == "ns:beta")  found_beta  = true;
        }
        CHECK(found_alpha);
        CHECK(found_beta);

        auto all_keys = bb.keys("");
        CHECK_GE(static_cast<int>(all_keys.size()), 3);
    }

    // ── Section 4: delete removes key ────────────────────────────────────────
    test::section("delete removes key from keys()");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        bb.write("del_me", 99);
        auto before = bb.keys("");
        CHECK_GE(static_cast<int>(before.size()), 1);

        bb.remove("del_me");
        auto after = bb.keys("");
        bool found = false;
        for (auto& k : after) { if (k == "del_me") found = true; }
        CHECK(!found);
    }

    // ── Section 5: AgentManager convenience wrappers ──────────────────────────
    test::section("AgentManager blackboard wrappers");
    {
        auto mgr = makeTestManager();

        CHECK_NOTHROW(mgr->blackboardWrite("mgr_key", {{"data", "hello"}}));
        auto val = mgr->blackboardRead("mgr_key");
        CHECK_EQ(val["data"].get<std::string>(), std::string("hello"));

        auto keys = mgr->blackboardKeys("mgr_");
        bool found = false;
        for (auto& k : keys) { if (k == "mgr_key") found = true; }
        CHECK(found);

        CHECK_NOTHROW(mgr->blackboardDelete("mgr_key"));
        auto keys2 = mgr->blackboardKeys("mgr_");
        bool still_there = false;
        for (auto& k : keys2) { if (k == "mgr_key") still_there = true; }
        CHECK(!still_there);
    }

    // ── Section 6: concurrent writes are thread-safe ─────────────────────────
    test::section("concurrent blackboard writes are safe");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        constexpr int N_THREADS = 8;
        constexpr int OPS_EACH  = 50;

        std::vector<std::thread> threads;
        threads.reserve(N_THREADS);

        for (int t = 0; t < N_THREADS; ++t) {
            threads.emplace_back([&bb, t]() {
                for (int i = 0; i < OPS_EACH; ++i) {
                    std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
                    bb.write(key, i);
                    (void)bb.keys("");
                }
            });
        }
        for (auto& th : threads) th.join();

        auto all = bb.keys("");
        CHECK_EQ(static_cast<int>(all.size()), N_THREADS * OPS_EACH);
    }

    // ── Section 7: Three agents write, synthesiser reads all three keys ────────
    // Models worked example 7: three research agents write findings to separate
    // blackboard keys; a synthesiser verifies all three are present.
    test::section("three agents write findings; synthesiser reads all three");
    {
        auto mgr = makeTestManager(nullptr, PDIR_C);
        test::registerEchoAction(mgr->factory());

        // Action that writes a value to the blackboard
        struct BBWriteAction : agent::Action {
            std::string key;
            nlohmann::json value;
            BBWriteAction(std::string id, nlohmann::json inp,
                          std::string k, nlohmann::json v)
                : agent::Action(std::move(id), "BBWriteAction", std::move(inp))
                , key(std::move(k)), value(std::move(v)) {}
            agent::WorkResult execute(agent::AgentContext& ctx) override {
                ctx.blackboard()->write(key, value);
                agent::WorkResult r;
                r.item_id   = id;
                r.item_name = name;
                r.item_kind = "Action";
                r.success   = true;
                r.output    = {{"wrote_key", key}};
                r.timestamp = std::chrono::system_clock::now();
                return r;
            }
        };

        mgr->factory().registerItem(
            agent::WorkItemSpec{"BBWriteAction", "Write to blackboard",
                                agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                std::string k = inp.value("key", "");
                nlohmann::json v = inp.value("value", nlohmann::json{});
                return std::make_unique<BBWriteAction>(
                    std::move(id), std::move(inp), std::move(k), std::move(v));
            }
        );

        // Spawn three worker agents
        agent::AgentConfig wcfg;
        wcfg.name = "worker"; wcfg.task = "write"; wcfg.max_iterations = 5;

        auto w1 = mgr->spawnAgent(wcfg);
        auto w2 = mgr->spawnAgent(wcfg);
        auto w3 = mgr->spawnAgent(wcfg);

        // Each writes a distinct key
        auto a1 = mgr->factory().create("BBWriteAction", "bba1",
            {{"key","findings/legal"},     {"value",{{"text","legal analysis"}}}});
        auto a2 = mgr->factory().create("BBWriteAction", "bba2",
            {{"key","findings/technical"}, {"value",{{"text","tech analysis"}}}});
        auto a3 = mgr->factory().create("BBWriteAction", "bba3",
            {{"key","findings/market"},    {"value",{{"text","market analysis"}}}});

        mgr->injectWork(w1, std::move(a1));
        mgr->injectWork(w2, std::move(a2));
        mgr->injectWork(w3, std::move(a3));

        // Run all three concurrently
        auto f1 = mgr->runAgent(w1, "write");
        auto f2 = mgr->runAgent(w2, "write");
        auto f3 = mgr->runAgent(w3, "write");
        f1.get(); f2.get(); f3.get();

        // Synthesiser: verify all three keys are present
        auto keys = mgr->blackboardKeys("findings/");
        CHECK_GE(static_cast<int>(keys.size()), 3);

        bool found_legal = false, found_tech = false, found_market = false;
        for (const auto& k : keys) {
            if (k == "findings/legal")     found_legal  = true;
            if (k == "findings/technical") found_tech   = true;
            if (k == "findings/market")    found_market = true;
        }
        CHECK(found_legal);
        CHECK(found_tech);
        CHECK(found_market);

        // Read back and verify content
        auto legal = mgr->blackboard().read("findings/legal");
        CHECK(legal.has_value());
        if (legal) {
            CHECK(legal->dump().find("legal analysis") != std::string::npos);
        }
    }

    // ── Section 8: contains() before and after write ──────────────────────────
    test::section("contains() reflects write/remove state");
    {
        auto bus = std::make_shared<agent::EventBus>();
        agent::Blackboard bb(bus.get());

        CHECK(!bb.contains("present_key"));
        bb.write("present_key", 1);
        CHECK(bb.contains("present_key"));
        bb.remove("present_key");
        CHECK(!bb.contains("present_key"));
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
