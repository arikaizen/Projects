#pragma once
#include "agent/llm_client.hpp"
#include "agent/work_item.hpp"
#include "agent/work_factory.hpp"
#include "agent/agent_context.hpp"
#include "agent/agent_manager.hpp"
#include "agent/memory_backend.hpp"
#include "agent/event_bus.hpp"
#include "agent/blackboard.hpp"
#include "agent/message_inbox.hpp"
#include "agent/thread_pool.hpp"
#include "agent/prompt_loader.hpp"
#include "agent/quota.hpp"
#include <iostream>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <thread>

namespace test {

static int pass_count  = 0;
static int fail_count  = 0;
static int total_count = 0;

static void record(bool ok, const char* expr, const char* file, int line) {
    ++total_count;
    if (ok) {
        ++pass_count;
        std::cout << "  [PASS] " << expr << "\n";
    } else {
        ++fail_count;
        std::cout << "  [FAIL] " << expr << "  <- " << file << ":" << line << "\n";
    }
}
static void section(const char* name) { std::cout << "\n-- " << name << " --\n"; }
static void summary() {
    std::cout << "\n==========================================\n"
              << "  Results: " << pass_count << " passed, "
              << fail_count << " failed, " << total_count << " total\n"
              << "==========================================\n";
}
static bool all_passed() { return fail_count == 0; }

} // namespace test

#define CHECK(expr)       test::record(!!(expr),    #expr,         __FILE__, __LINE__)
#define CHECK_EQ(a,b)     test::record((a)==(b),    #a " == " #b,  __FILE__, __LINE__)
#define CHECK_NE(a,b)     test::record((a)!=(b),    #a " != " #b,  __FILE__, __LINE__)
#define CHECK_GT(a,b)     test::record((a)>(b),     #a " > "  #b,  __FILE__, __LINE__)
#define CHECK_GE(a,b)     test::record((a)>=(b),    #a " >= " #b,  __FILE__, __LINE__)
#define CHECK_THROW(expr) do { bool _threw=false; try { (expr); } catch(...){ _threw=true; } \
    test::record(_threw, "THROWS: " #expr, __FILE__, __LINE__); } while(0)
#define CHECK_NOTHROW(expr) do { bool _threw=false; try { (expr); } catch(...){ _threw=true; } \
    test::record(!_threw, "NOTHROW: " #expr, __FILE__, __LINE__); } while(0)

// ── EchoAction helper ────────────────────────────────────────────────────────
// Resolves its inputs via ctx.resolveReferences and returns them as output.
// Used throughout the test suite wherever a deterministic no-I/O Action is needed.
#include "agent/action.hpp"
#include <filesystem>
#include <fstream>

namespace test {

struct EchoAction : agent::Action {
    EchoAction(std::string id_, nlohmann::json inp)
        : agent::Action(std::move(id_), "EchoAction", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        auto resolved = ctx.resolveReferences(inputs);
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = resolved;
        r.timestamp = std::chrono::system_clock::now();
        r.duration  = std::chrono::milliseconds(0);
        return r;
    }
};

// Register EchoAction into a WorkFactory.
inline void registerEchoAction(agent::WorkFactory& factory) {
    factory.registerItem(
        agent::WorkItemSpec{
            "EchoAction",
            "Echo resolved inputs as output (test helper)",
            agent::WorkItem::Kind::Action,
            {{"type","object"}}
        },
        [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
            return std::make_unique<EchoAction>(std::move(id), std::move(inp));
        }
    );
}

// Write the four stub prompt templates into prompts_dir so PromptLoader won't throw.
inline void writeStubPrompts(const std::string& prompts_dir) {
    namespace fs = std::filesystem;
    fs::create_directories(prompts_dir);

    auto write = [&](const char* name, const char* body) {
        std::string path = prompts_dir + "/" + name + ".md";
        if (!fs::exists(path)) {
            std::ofstream f(path);
            f << body;
        }
    };

    write("reason_stage",
          "CATALOG:{{CATALOG}}\nHISTORY:{{HISTORY}}\nQUEUE:{{QUEUE}}\n"
          "TASK:{{TASK}}\nSCHEMA:{{OUTPUT_SCHEMA}}\n");

    write("injection_stage",
          "CATALOG:{{CATALOG}}\nHISTORY:{{HISTORY}}\nQUEUE:{{QUEUE}}\n"
          "TASK:{{TASK}}\nPREVIOUS:{{PREVIOUS_RESULT}}\nSCHEMA:{{OUTPUT_SCHEMA}}\n");

    write("transform_stage",
          "INSTRUCTION:{{INSTRUCTION}}\nINPUT:{{INPUT_TEXT}}\n");

    write("validate_stage",
          "TARGET:{{TARGET_OUTPUT}}\nCRITERIA:{{CRITERIA}}\n");
}

} // namespace test

// ── makeTestManager ──────────────────────────────────────────────────────────
// Build a minimal AgentManager for tests:
//   - MockLLMClient with a caller-supplied handler (default: returns empty plan "[]")
//   - NoOpMemoryBackend
//   - Stub prompt files written to prompts_dir
//   - thread_pool_size = 4
inline std::shared_ptr<agent::AgentManager> makeTestManager(
    agent::LLMClient::Handler llm_handler = nullptr,
    const std::string& prompts_dir = "/tmp/agent_test_prompts")
{
    test::writeStubPrompts(prompts_dir);

    if (!llm_handler) {
        llm_handler = [](const agent::LLMClient::Request&) -> agent::LLMClient::Response {
            return {"[]", true, ""};  // empty plan by default
        };
    }

    auto llm = std::make_shared<agent::MockLLMClient>(llm_handler);
    auto mem = std::make_shared<agent::NoOpMemoryBackend>();

    agent::AgentManager::Config cfg;
    cfg.prompts_dir      = prompts_dir;
    cfg.thread_pool_size = 4;

    return std::make_shared<agent::AgentManager>(cfg, llm, mem);
}

// ── makeTestContext ───────────────────────────────────────────────────────────
// Build a raw AgentContext directly (useful for unit tests that don't need the
// full AgentManager).  Caller owns the returned unique_ptr; the EventBus and
// Blackboard shared_ptrs must stay alive for the context's lifetime.
inline std::unique_ptr<agent::AgentContext> makeTestContext(
    const std::string& agent_id = "test_agent",
    const std::string& task     = "test_task",
    agent::LLMClient::Handler llm_handler = nullptr,
    std::shared_ptr<agent::EventBus>   bus = nullptr,
    std::shared_ptr<agent::Blackboard> bb  = nullptr)
{
    if (!llm_handler) {
        llm_handler = [](const agent::LLMClient::Request&) -> agent::LLMClient::Response {
            return {"[]", true, ""};
        };
    }
    if (!bus) bus = std::make_shared<agent::EventBus>();
    if (!bb)  bb  = std::make_shared<agent::Blackboard>(bus.get());

    // AgentContext stores raw (non-owning) pointers to the event bus, blackboard
    // and inbox.  In production these are owned by the AgentManager and outlive
    // every context; here they have no other owner, so keep them alive for the
    // lifetime of the test process.  (Test-only; the process is short-lived.)
    static std::vector<std::shared_ptr<void>> s_keepalive;
    s_keepalive.push_back(bus);
    s_keepalive.push_back(bb);

    auto llm     = std::make_shared<agent::MockLLMClient>(llm_handler);
    auto factory = std::make_shared<agent::WorkFactory>();
    auto loader  = std::make_shared<agent::PromptLoader>("/tmp");
    auto mem     = std::make_shared<agent::NoOpMemoryBackend>();
    auto inbox   = std::make_shared<agent::MessageInbox>();
    s_keepalive.push_back(inbox);

    agent::AgentConfig cfg;
    cfg.agent_id = agent_id;
    cfg.task     = task;

    return std::make_unique<agent::AgentContext>(
        cfg, llm, factory, loader, mem,
        bb.get(), inbox.get(), bus.get(), nullptr);
}
