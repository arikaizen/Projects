// test_pattern_b_messaging.cpp — Pattern B: agent messaging (req 43-45, worked example 6)
//
// Covers:
//   - sendMessage from one agent to another
//   - drainInbox returns sent messages (payload field)
//   - broadcast delivers to all other agents
//   - MessageInbox direct thread-safety

#include "test_helper.hpp"
#include <thread>
#include <vector>

static const std::string PDIR_B = "/tmp/agent_pattern_b_prompts";

int main() {
    std::cout << "=== test_pattern_b_messaging ===\n";

    test::writeStubPrompts(PDIR_B);

    // ── Section 1: sendMessage + drainInbox ───────────────────────────────────
    test::section("sendMessage and drainInbox");
    {
        auto mgr = makeTestManager(nullptr, PDIR_B);
        test::registerEchoAction(mgr->factory());

        agent::AgentConfig cfg;
        cfg.name = "Sender";
        cfg.task = "s";
        std::string sid = mgr->spawnAgent(cfg);

        cfg.name = "Receiver";
        cfg.task = "r";
        std::string rid = mgr->spawnAgent(cfg);

        nlohmann::json msg = {{"text", "hello from sender"}};
        CHECK_NOTHROW(mgr->sendMessage(sid, rid, msg));

        auto inbox = mgr->drainInbox(rid);
        CHECK_GE(static_cast<int>(inbox.size()), 1);
        if (!inbox.empty()) {
            // Message.payload is the nlohmann::json field (not .body)
            bool found = false;
            for (const auto& m : inbox) {
                std::string dump = m.payload.dump();
                if (dump.find("hello from sender") != std::string::npos) found = true;
            }
            CHECK(found);
        }

        mgr->destroyAgent(sid);
        mgr->destroyAgent(rid);
    }

    // ── Section 2: drainInbox clears the inbox ────────────────────────────────
    test::section("drainInbox empties the inbox");
    {
        auto mgr = makeTestManager(nullptr, PDIR_B);
        test::registerEchoAction(mgr->factory());

        agent::AgentConfig cfg;
        cfg.name = "A"; cfg.task = "t";
        std::string aid = mgr->spawnAgent(cfg);
        cfg.name = "B";
        std::string bid = mgr->spawnAgent(cfg);

        mgr->sendMessage(aid, bid, {{"n", 1}});
        mgr->sendMessage(aid, bid, {{"n", 2}});

        auto first_drain = mgr->drainInbox(bid);
        CHECK_EQ(static_cast<int>(first_drain.size()), 2);

        auto second_drain = mgr->drainInbox(bid);
        CHECK_EQ(static_cast<int>(second_drain.size()), 0);

        mgr->destroyAgent(aid);
        mgr->destroyAgent(bid);
    }

    // ── Section 3: broadcast delivers to all other agents ────────────────────
    test::section("broadcast delivers to all other inboxes");
    {
        auto mgr = makeTestManager(nullptr, PDIR_B);
        test::registerEchoAction(mgr->factory());

        agent::AgentConfig cfg;
        cfg.name = "Broadcaster"; cfg.task = "t";
        std::string broadcaster = mgr->spawnAgent(cfg);
        cfg.name = "R1";
        std::string r1 = mgr->spawnAgent(cfg);
        cfg.name = "R2";
        std::string r2 = mgr->spawnAgent(cfg);

        nlohmann::json bcast = {{"announcement", "hello all"}};
        CHECK_NOTHROW(mgr->broadcast(broadcaster, bcast));

        auto inbox1 = mgr->drainInbox(r1);
        auto inbox2 = mgr->drainInbox(r2);

        // Both receivers should have gotten the broadcast
        CHECK_GE(static_cast<int>(inbox1.size()), 1);
        CHECK_GE(static_cast<int>(inbox2.size()), 1);

        // Broadcaster itself should not get its own broadcast
        auto self_inbox = mgr->drainInbox(broadcaster);
        bool self_got_own = false;
        for (const auto& m : self_inbox) {
            // m.payload is nlohmann::json
            if (m.payload.dump().find("hello all") != std::string::npos) self_got_own = true;
        }
        CHECK(!self_got_own);

        mgr->destroyAgent(broadcaster);
        mgr->destroyAgent(r1);
        mgr->destroyAgent(r2);
    }

    // ── Section 4: MessageInbox direct unit tests ─────────────────────────────
    test::section("MessageInbox direct unit tests");
    {
        agent::MessageInbox inbox;
        CHECK_EQ(static_cast<int>(inbox.drain().size()), 0);
        CHECK(inbox.empty());

        // Message struct uses payload (not body)
        agent::Message m1{};
        m1.from_id  = "a";
        m1.to_id    = "b";
        m1.payload  = {{"x", 1}};
        m1.timestamp = "2025-01-01T00:00:00Z";

        agent::Message m2{};
        m2.from_id  = "a";
        m2.to_id    = "b";
        m2.payload  = {{"x", 2}};
        m2.timestamp = "2025-01-01T00:00:01Z";

        inbox.push(m1);
        CHECK(!inbox.empty());
        inbox.push(m2);

        auto msgs = inbox.drain();
        CHECK_EQ(static_cast<int>(msgs.size()), 2);
        CHECK(inbox.empty());

        // Drain again should be empty
        CHECK_EQ(static_cast<int>(inbox.drain().size()), 0);
    }

    // ── Section 5: Coordinator → worker messaging end-to-end ─────────────────
    test::section("coordinator sends message to worker inbox");
    {
        auto mgr = makeTestManager(nullptr, PDIR_B);
        test::registerEchoAction(mgr->factory());

        agent::AgentConfig coord_cfg;
        coord_cfg.name = "coordinator";
        coord_cfg.task = "coord";

        agent::AgentConfig worker_cfg;
        worker_cfg.name = "worker";
        worker_cfg.task = "work";

        auto coord_id  = mgr->spawnAgent(coord_cfg);
        auto worker_id = mgr->spawnAgent(worker_cfg);

        // Coordinator sends a task payload to worker
        nlohmann::json task_msg = {
            {"type",    "task_assignment"},
            {"payload", "analyze document X"}
        };
        mgr->sendMessage(coord_id, worker_id, task_msg);

        // Worker drains its inbox
        auto msgs = mgr->drainInbox(worker_id);
        CHECK_GE(static_cast<int>(msgs.size()), 1);
        if (!msgs.empty()) {
            bool found_task = false;
            for (const auto& m : msgs) {
                if (m.payload.dump().find("task_assignment") != std::string::npos)
                    found_task = true;
            }
            CHECK(found_task);
            // from_id should be the coordinator
            CHECK_EQ(msgs[0].from_id, coord_id);
            CHECK_EQ(msgs[0].to_id,   worker_id);
        }
    }

    // ── Section 6: concurrent pushes to MessageInbox are thread-safe ──────────
    test::section("concurrent inbox pushes are thread-safe");
    {
        agent::MessageInbox inbox;
        const int N = 100;
        std::vector<std::thread> threads;
        for (int i = 0; i < N; ++i) {
            threads.emplace_back([&inbox, i]() {
                agent::Message m;
                m.from_id = "t" + std::to_string(i);
                m.to_id   = "target";
                m.payload = {{"i", i}};
                inbox.push(m);
            });
        }
        for (auto& t : threads) t.join();

        auto drained = inbox.drain();
        CHECK_EQ(static_cast<int>(drained.size()), N);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
