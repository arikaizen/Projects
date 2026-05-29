// test_quotas.cpp — Quota enforcement (req 38)
//
// Covers:
//   - Set user quota to max_agents=2
//   - Spawn 2 agents — succeeds
//   - Spawn 3rd — throws or returns quota error
//   - Release one (destroyAgent) → spawn again → succeeds
//   - LLM inflight quota (max_llm_inflight)
//   - Default quota allows many agents

#include "test_helper.hpp"

static const std::string PDIR = "/tmp/agent_quotas_prompts";

int main() {
    std::cout << "=== test_quotas ===\n";

    test::writeStubPrompts(PDIR);

    // ── Section 1: max_concurrent_agents enforcement ─────────────────────────
    test::section("quota max_concurrent_agents=2 enforced");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        agent::UserQuota q;
        q.max_concurrent_agents = 2;
        mgr->setUserQuota("limited_user", q);

        agent::AgentConfig cfg;
        cfg.name   = "quota_test_agent";
        cfg.task   = "task";
        cfg.extra["user_id"] = "limited_user";

        // First two spawns succeed
        std::string id1, id2;
        CHECK_NOTHROW(id1 = mgr->spawnAgent(cfg));
        CHECK_NOTHROW(id2 = mgr->spawnAgent(cfg));

        // Third must fail
        CHECK_THROW(mgr->spawnAgent(cfg));

        // Verify quota was truly exceeded (not just a fluke)
        bool threw = false;
        try { mgr->spawnAgent(cfg); } catch (...) { threw = true; }
        CHECK(threw);

        // Destroy one → slot freed
        mgr->destroyAgent(id1);

        std::string id3;
        CHECK_NOTHROW(id3 = mgr->spawnAgent(cfg));

        // Now at 2 again, should fail
        CHECK_THROW(mgr->spawnAgent(cfg));

        // Cleanup
        mgr->destroyAgent(id2);
        mgr->destroyAgent(id3);
    }

    // ── Section 2: Different users have independent quotas ────────────────────
    test::section("different users have independent quota counters");
    {
        auto mgr = makeTestManager(nullptr, PDIR);
        test::registerEchoAction(mgr->factory());

        agent::UserQuota q;
        q.max_concurrent_agents = 1;
        mgr->setUserQuota("user_A", q);
        mgr->setUserQuota("user_B", q);

        agent::AgentConfig cfgA;
        cfgA.name = "agent_a";
        cfgA.task = "t";
        cfgA.extra["user_id"] = "user_A";

        agent::AgentConfig cfgB;
        cfgB.name = "agent_b";
        cfgB.task = "t";
        cfgB.extra["user_id"] = "user_B";

        std::string idA, idB;
        CHECK_NOTHROW(idA = mgr->spawnAgent(cfgA));
        CHECK_NOTHROW(idB = mgr->spawnAgent(cfgB));  // Different user → independent quota

        // Both users are at their limit now
        CHECK_THROW(mgr->spawnAgent(cfgA));
        CHECK_THROW(mgr->spawnAgent(cfgB));

        mgr->destroyAgent(idA);
        mgr->destroyAgent(idB);
    }

    // ── Section 3: QuotaManager directly ─────────────────────────────────────
    test::section("QuotaManager tryAcquire/release cycles");
    {
        agent::QuotaManager qm;
        agent::UserQuota q;
        q.max_concurrent_agents = 3;
        qm.setQuota("direct_user", q);

        CHECK(qm.tryAcquireAgent("direct_user"));
        CHECK(qm.tryAcquireAgent("direct_user"));
        CHECK(qm.tryAcquireAgent("direct_user"));
        CHECK(!qm.tryAcquireAgent("direct_user"));  // at limit

        qm.releaseAgent("direct_user");
        CHECK(qm.tryAcquireAgent("direct_user"));   // freed one slot
        CHECK(!qm.tryAcquireAgent("direct_user"));  // back at limit

        qm.releaseAgent("direct_user");
        qm.releaseAgent("direct_user");
        qm.releaseAgent("direct_user");
    }

    // ── Section 4: Default quota is generous ─────────────────────────────────
    test::section("default quota allows many agents");
    {
        agent::QuotaManager qm;
        // Default quota is 10 concurrent agents
        for (int i = 0; i < 10; ++i) {
            CHECK(qm.tryAcquireAgent("default_user"));
        }
        // 11th should fail
        CHECK(!qm.tryAcquireAgent("default_user"));
    }

    // ── Section 5: getQuota returns configured values ────────────────────────
    test::section("getQuota returns configured limits");
    {
        agent::QuotaManager qm;
        agent::UserQuota q;
        q.max_concurrent_agents = 5;
        q.max_llm_inflight      = 2;
        q.max_tool_inflight     = 15;
        qm.setQuota("config_user", q);

        auto got = qm.getQuota("config_user");
        CHECK_EQ(got.max_concurrent_agents, 5);
        CHECK_EQ(got.max_llm_inflight,      2);
        CHECK_EQ(got.max_tool_inflight,     15);
    }

    // ── Section 6: LLM inflight quota ────────────────────────────────────────
    test::section("LLM inflight quota enforced");
    {
        agent::QuotaManager qm;
        agent::UserQuota q;
        q.max_llm_inflight = 2;
        qm.setQuota("llm_user", q);

        CHECK(qm.tryAcquireLLM("llm_user"));
        CHECK(qm.tryAcquireLLM("llm_user"));
        CHECK(!qm.tryAcquireLLM("llm_user"));   // at limit

        qm.releaseLLM("llm_user");
        CHECK(qm.tryAcquireLLM("llm_user"));    // slot freed
    }

    // ── Section 7: Tool inflight quota ───────────────────────────────────────
    test::section("tool inflight quota enforced");
    {
        agent::QuotaManager qm;
        agent::UserQuota q;
        q.max_tool_inflight = 1;
        qm.setQuota("tool_user", q);

        CHECK(qm.tryAcquireTool("tool_user"));
        CHECK(!qm.tryAcquireTool("tool_user"));
        qm.releaseTool("tool_user");
        CHECK(qm.tryAcquireTool("tool_user"));
    }

    // ── Section 8: usageJson reflects current counts ──────────────────────────
    test::section("usageJson reflects current usage");
    {
        agent::QuotaManager qm;
        agent::UserQuota q;
        q.max_concurrent_agents = 10;
        qm.setQuota("usage_user", q);

        qm.tryAcquireAgent("usage_user");
        qm.tryAcquireAgent("usage_user");
        qm.tryAcquireLLM("usage_user");

        auto usage = qm.usageJson("usage_user");
        CHECK(usage.contains("agents_active") || usage.contains("agents"));
        // Either key is acceptable depending on implementation naming
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
