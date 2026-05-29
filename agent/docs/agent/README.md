# Agent Engine

A C++17 multi-agent orchestration engine with a clean C ABI for GUI integration.

---

## 1. Architecture Overview

```
GUI / C ABI (agent_engine/c_api.h)
         ↓
AgentManager (central orchestrator)
├── ThreadPool           (L1/L2 concurrency: shared worker threads)
├── WorkFactory          (item registry: Actions and Stages)
├── PromptLoader         (template loading + {{PLACEHOLDER}} substitution)
├── Blackboard           (Pattern C: shared key-value store)
├── EventBus             (live event dispatch to subscribers)
├── QuotaManager         (per-user resource limits)
└── [per agent] Agent
    ├── AgentContext     (queue, history, termination flags)
    └── BatchExecutor    (L3 concurrency: dependency-aware DAG execution)
```

Each `Agent` is an independent task runner. Multiple agents can run concurrently
on the shared `ThreadPool`. Within a single agent, the `BatchExecutor` executes
independent items in parallel while serialising items that have data dependencies.

---

## 2. Concurrency Model — The Four Levels

### L1: Many agents on the shared thread pool (shared-nothing)

`AgentManager::runAgent()` submits each agent's `run()` call to the shared
`ThreadPool`. Agents do not share queues or history. The only shared state is
the `Blackboard`, `EventBus`, and the `WorkFactory` (all individually
thread-safe).

### L2: Sub-agents — Pattern A fan-out

An action can spawn child agents via `AgentManager::spawnAgent()` and
`fanOut()`. Each child runs on the same pool. Results are collected via
`std::future` (C++ API) or `AgentFuture*` (C ABI).

**Starvation guard:** with `N` pool threads and `D` nesting levels, you need at
least `D + 1` threads to prevent the pool from deadlocking. Configure
`thread_pool_size` accordingly, or use the continuation-based `fanIn()` which
does not hold a thread while waiting for children.

### L3: BatchExecutor DAG within one agent

After a Stage pushes a plan, the agent loop drains all queued items and submits
them to `BatchExecutor` as a single batch. Independent items (no data
dependencies between them) are submitted concurrently to the pool. Items that
reference another item's output (`$id` or `$id.field`) are held until their
dependency completes.

Algorithm (Kahn's topological sort + polling):
1. Build DAG: for each item, scan its inputs for `$ref` strings. If `ref` is
   in history, no edge is needed. If `ref` is in the batch, add an edge.
   If `ref` is unresolvable, reject the batch with an error.
2. Detect cycles via Kahn's algorithm. Throw `std::runtime_error` on cycle.
3. Submit all zero-in-degree nodes to the pool concurrently.
4. Poll for completed futures. On completion, update dependents; submit any
   newly-ready items.
5. On failure, skip all transitive dependents and record `skipped_reason`.
6. On cancellation (`ctx.cancellation_flag`), do not start new items.
7. Record results into `ctx.history()` in *declared order* (not completion order).

### L4: Sequential reasoning — stages run one-at-a-time

Stages (ReasonStage, InjectionStage, …) run as a single-item batch. This
ensures each stage sees the full prior history before planning its next step.
Action batches may run in parallel; Stage batches are always size-1.

---

## 3. BatchExecutor Algorithm — Worked Example

Items: `[a, b(deps a), c(deps a), d(deps b, c)]`

```
Step 0: ready={a}               in_flight={}
Step 1: submit a                in_flight={a}
Step 2: a finishes              ready={b, c}
Step 3: submit b, c concurrently in_flight={b, c}  peak_concurrency=2
Step 4: b finishes              ready={}           (c still running)
Step 5: c finishes              ready={d}
Step 6: submit d                in_flight={d}
Step 7: d finishes              done

History recorded: a, b, c, d  (declared order — NOT completion order)
```

---

## 4. Thread-Safety Classification

| Component            | Primitive                 | Notes                                         |
|----------------------|---------------------------|-----------------------------------------------|
| `WorkFactory`        | `std::shared_mutex`       | Frequent reads (create), rare writes (register) |
| `PromptLoader`       | `std::mutex`              | Cache reads + disk fallback                   |
| `Blackboard`         | `std::mutex`              | Per-key sharding planned for v2               |
| Per-agent queue      | `std::mutex` + condvar    | `push()` wakes `pop()`                        |
| `cancellation_flag`  | `std::atomic<bool>`       | Lock-free read from any thread                |
| `EventBus`           | `std::mutex` (subscriber list) | Dispatch copies list; callbacks lock-free |
| `LLM client`         | Stateless / impl-defined  | `MockLLMClient` is a pure function            |
| `MemoryBackend`      | Impl-defined              | `NoOpMemoryBackend` is trivially safe         |
| `ThreadPool`         | `std::mutex` + condvar    | Task queue; `submit()` always thread-safe     |
| `MessageInbox`       | `std::mutex`              | `push()` / `drain()` are atomic               |
| `QuotaManager`       | `std::mutex`              | `tryAcquire` / `release` are atomic           |

---

## 5. How to Register a New Action

```cpp
#include "agent/action.hpp"
#include "agent/work_factory.hpp"

// 1. Subclass Action (or WorkItem directly)
struct MyAction : agent::Action {
    MyAction(std::string id, nlohmann::json inp)
        : agent::Action(std::move(id), "MyAction", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        // Resolve $references in inputs
        auto resolved = ctx.resolveReferences(inputs);

        // … do work …

        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.output    = {{"result", "done"}};
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};

// 2. Register with the factory (call once at startup)
void registerMyAction(agent::WorkFactory& factory) {
    factory.registerItem(
        agent::WorkItemSpec{
            "MyAction",
            "Does something useful",
            agent::WorkItem::Kind::Action,
            {{"type","object"},{"properties",{{"arg",{{"type","string"}}}}}}
        },
        [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
            return std::make_unique<MyAction>(std::move(id), std::move(inp));
        }
    );
}
```

---

## 6. How to Register a New Stage

Stages are identical to Actions except they return `Kind::Stage` and typically
call `ctx.llm().complete()` and then call `ctx.push()` to enqueue follow-up items.

```cpp
#include "agent/stage.hpp"

struct MyStage : agent::Stage {
    MyStage(std::string id, nlohmann::json inp)
        : agent::Stage(std::move(id), "MyStage", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext& ctx) override {
        // Build prompt, call LLM
        std::string prompt = ctx.promptLoader().render("my_stage", {
            {"TASK", ctx.config().task},
            // … other placeholders …
        });

        auto resp = ctx.llm().complete({prompt, "produce plan", true});
        if (!resp.success) {
            /* handle error */
        }

        // Parse plan and push items
        auto plan = nlohmann::json::parse(resp.content);
        for (auto& item : plan) {
            auto work = ctx.factory().create(
                item["name"], item["id"], item["inputs"]);
            ctx.push(std::move(work), agent::AgentContext::Position::Back);
        }

        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Stage";
        r.success   = true;
        r.output    = {{"plan_size", plan.size()}};
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};
```

---

## 7. Prompt Template Writing

Templates are Markdown files in the prompts directory. Placeholders use the
syntax `{{KEY}}` (uppercase identifier in double braces).

### Per-stage placeholder contracts

| Stage              | Required placeholders                                                |
|--------------------|----------------------------------------------------------------------|
| `reason_stage`     | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{OUTPUT_SCHEMA}}` |
| `injection_stage`  | `{{CATALOG}}`, `{{HISTORY}}`, `{{QUEUE}}`, `{{TASK}}`, `{{PREVIOUS_RESULT}}`, `{{OUTPUT_SCHEMA}}` |
| `transform_stage`  | `{{INSTRUCTION}}`, `{{INPUT_TEXT}}`                                 |
| `validate_stage`   | `{{TARGET_OUTPUT}}`, `{{CRITERIA}}`                                  |

Placeholders with no matching key in `vars` cause `substitute()` to throw
`std::runtime_error`. Extra keys in `vars` are silently ignored.

### Example well-formed reason_stage.md

```markdown
# Agent Reasoning

You are an autonomous agent. Your goal is to complete the following task:

**Task:** {{TASK}}

## Available Actions

{{CATALOG}}

## Execution History

{{HISTORY}}

## Current Queue

{{QUEUE}}

## Output Format

Respond with a JSON array matching the schema below. Each element is one work item.
If the task is complete, include `"final_answer"` on the last item.

{{OUTPUT_SCHEMA}}
```

---

## 8. Templates: Location, Override, Hot Reload

- Default location: `./prompts` (relative to the working directory at startup)
- Override at construction: `AgentManager::Config::prompts_dir`
- Override at runtime (C++): `mgr.setPromptsDir(path)` — implies `reload()`
- Hot reload (C++): `mgr.reloadPrompts()` — clears the in-memory cache; next
  call to `render()` re-reads from disk
- Hot reload (C ABI): `am_reload_prompts(mgr)` / `am_set_prompts_dir(mgr, path)`

---

## 9. Patterns A / B / C

### Pattern A — Delegation / Piping

```cpp
// C++
auto researcher = mgr.spawnAgent({.name="researcher", .task="research AI Act"});
auto writer     = mgr.spawnAgent({.name="writer"});
mgr.pipe(researcher, writer, "Summarise: {{OUTPUT}}");
auto fut = mgr.runAgent(researcher, "research AI Act");
auto result = fut.get();  // writer receives the pipe message automatically
```

```c
// C ABI
char rid[256], wid[256];
am_spawn_agent(mgr, "{\"name\":\"researcher\"}", rid, sizeof(rid));
am_spawn_agent(mgr, "{\"name\":\"writer\"}",     wid, sizeof(wid));
am_pipe(mgr, rid, wid, "Summarise: {{OUTPUT}}");
AgentFuture* fut = am_run_agent(mgr, rid, "{\"task\":\"research AI Act\"}");
am_future_wait(fut, -1, buf, sizeof(buf));
am_future_free(fut);
```

### Pattern B — Messaging

```cpp
// C++
mgr.sendMessage(coordinator_id, worker_id, {{"type","task"},{"data","…"}});
auto msgs = mgr.drainInbox(worker_id);
```

```c
// C ABI
am_send_message(mgr, coord_id, worker_id, "{\"type\":\"task\"}");
am_drain_inbox(mgr, worker_id, buf, sizeof(buf));
```

### Pattern C — Blackboard

```cpp
// C++
mgr.blackboardWrite("findings/legal",     {{"text","legal analysis"}});
mgr.blackboardWrite("findings/technical", {{"text","tech analysis"}});
mgr.blackboardWrite("findings/market",    {{"text","market analysis"}});
auto keys = mgr.blackboardKeys("findings/");
for (const auto& k : keys) {
    auto val = mgr.blackboardRead(k);
}
```

```c
// C ABI
am_blackboard_write(mgr, "findings/legal",     "{\"text\":\"legal\"}");
am_blackboard_write(mgr, "findings/technical", "{\"text\":\"tech\"}");
am_blackboard_read(mgr,  "findings/legal",     buf, sizeof(buf));
```

---

## 10. Multi-Tenancy and Quotas

Each agent is associated with a `user_id` (from `AgentConfig::extra["user_id"]`
or `AgentManager::Config::default_user_id`). The `QuotaManager` enforces:

| Limit                    | Default |
|--------------------------|---------|
| `max_concurrent_agents`  | 10      |
| `max_llm_inflight`       | 5       |
| `max_tool_inflight`      | 20      |

```cpp
agent::UserQuota q;
q.max_concurrent_agents = 5;
q.max_llm_inflight      = 2;
mgr.setUserQuota("alice", q);
```

Exceeding `max_concurrent_agents` throws `std::runtime_error` from `spawnAgent()`
(C++) or returns `AM_ERROR_QUOTA_EXCEEDED` (C ABI).

---

## 11. Running Tests

```bash
# Standard build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build

# With ThreadSanitizer
cmake -B build-tsan \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan
ctest --test-dir build-tsan

# With AddressSanitizer
cmake -B build-asan \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan
ctest --test-dir build-asan
```

All tests in `tests/agent/` use `MockLLMClient` and require no real API access.
`test_c_abi.cpp` links against `libagent_engine.so` and exercises the full C ABI.

---

## 12. Requirement-to-File Traceability

| Req / Example | File(s)                                                                     |
|---------------|-----------------------------------------------------------------------------|
| 1             | `include/agent/work_item.hpp`, `src/agent/agent.cpp`                        |
| 2             | `src/agent/stages/reason_stage.cpp`, `tests/agent/test_injection_reading1.cpp` |
| 3             | `src/agent/stages/injection_stage.cpp`, `tests/agent/test_injection_reading2.cpp` |
| 4             | `include/agent/agent_context.hpp`, `src/agent/agent_context.cpp`            |
| 5             | `src/agent/agent_context.cpp` (resolveReferences), `tests/agent/test_references.cpp` |
| 6             | `src/agent/agent_context.cpp`, `tests/agent/test_chains.cpp`                |
| 7             | `include/agent/work_factory.hpp`, `src/agent/work_factory.cpp`              |
| 8             | `tests/agent/test_factory.cpp`                                              |
| 9             | `include/agent/prompt_loader.hpp`, `src/agent/prompt_loader.cpp`            |
| 10            | `tests/agent/test_prompt_loader.cpp`                                        |
| 11            | `include/agent/llm_client.hpp`                                              |
| 12            | `include/agent/memory_backend.hpp`                                          |
| 13            | `include/agent/blackboard.hpp`, `src/agent/blackboard.cpp`                  |
| 14            | `include/agent/event_bus.hpp`, `src/agent/event_bus.cpp`                    |
| 15            | `src/agent/batch_executor.cpp` (Kahn's), `tests/agent/test_cycle_detection.cpp` |
| 16            | `src/agent/agent.cpp`, `tests/agent/test_agent_loop.cpp`                    |
| 17            | `include/agent/agent.hpp` (TerminationReason enum)                          |
| 18            | `include/agent/agent_context.hpp` (cancellation_flag), `tests/agent/test_cancellation.cpp` |
| 19            | `src/agent/agent.cpp` (cancellation check in loop)                          |
| 20            | `src/agent/agent_context.cpp` (injectFromOutside), `tests/agent/test_real_time_injection.cpp` |
| 21            | `include/agent/agent_manager.hpp` (injectWork)                              |
| 22            | `src/agent/agent_manager.cpp` (injectWork)                                  |
| 23            | `include/agent/work_item.hpp` (dependencies())                              |
| 24            | `src/agent/work_item.cpp` (scanForRefs)                                     |
| 25            | `include/agent/batch_executor.hpp`                                          |
| 26            | `src/agent/batch_executor.cpp` (buildDAG)                                   |
| 27            | `src/agent/batch_executor.cpp` (execute — parallel submit)                  |
| 28            | `src/agent/batch_executor.cpp` (failure propagation)                        |
| 29            | `src/agent/batch_executor.cpp` (deterministic merge at end)                 |
| 30            | `tests/agent/test_determinism.cpp`                                          |
| 31            | `tests/agent/test_dependency_dag.cpp`                                       |
| 32            | `tests/agent/test_starvation.cpp`                                           |
| 33            | `tests/agent/test_batch_executor.cpp`                                       |
| 34            | `include/agent/thread_pool.hpp`, `src/agent/thread_pool.cpp`                |
| 35            | `src/agent/batch_executor.cpp` (declared-order history merge)               |
| 36            | `include/agent/agent_context.hpp` (history())                               |
| 37            | `src/agent/agent_context.cpp` (recordResult — mutex-protected)              |
| 38            | `include/agent/quota.hpp`, `src/agent/quota.cpp`, `tests/agent/test_quotas.cpp` |
| 39            | `tests/agent/test_cancellation.cpp`, `tests/agent/test_concurrency.cpp`     |
| 40            | `src/agent/agent_manager.cpp` (spawnAgent, runAgent)                        |
| 41            | `src/agent/agent_manager.cpp` (destroyAgent)                                |
| 42            | `src/agent/agent_manager.cpp` (pipe, onAgentFinished)                       |
| 43            | `src/agent/agent_manager.cpp` (sendMessage)                                 |
| 44            | `src/agent/agent_manager.cpp` (broadcast)                                   |
| 45            | `src/agent/agent_manager.cpp` (drainInbox), `tests/agent/test_pattern_b_messaging.cpp` |
| 46            | `src/agent/blackboard.cpp` (write/read/remove/keys)                         |
| 47            | `src/agent/agent_manager.cpp` (blackboardWrite/Read/Keys/Delete)            |
| 48            | `tests/agent/test_pattern_c_blackboard.cpp`                                 |
| 49            | `src/agent/agent_manager.cpp` (fanOut/fanIn/researchFromAngles)             |
| 50            | `include/agent_engine/c_api.h` (ABI design)                                 |
| 51            | `src/agent_engine/c_api.cpp` (am_create/am_destroy)                         |
| 52            | `src/agent_engine/c_api.cpp` (am_spawn_agent/am_destroy_agent)              |
| 53            | `src/agent_engine/c_api.cpp` (am_run_agent/am_future_wait/am_future_free)   |
| 54            | `src/agent_engine/c_api.cpp` (am_pipe)                                      |
| 55            | `src/agent_engine/c_api.cpp` (am_send_message/am_drain_inbox)               |
| 56            | `src/agent_engine/c_api.cpp` (am_blackboard_write/read/keys)                |
| 57            | `src/agent_engine/c_api.cpp` (am_fan_out/am_research_from_angles)           |
| 58            | `src/agent_engine/c_api.cpp` (am_inject_work)                               |
| 59            | `src/agent_engine/c_api.cpp` (am_reload_prompts/am_set_prompts_dir)         |
| 60            | `src/agent_engine/c_api.cpp` (am_set_user_quota), `tests/agent/test_c_abi.cpp` |
| Example 1     | `tests/agent/test_chains.cpp`                                               |
| Example 2     | `tests/agent/test_injection_reading1.cpp`                                   |
| Example 3     | `tests/agent/test_injection_reading2.cpp`                                   |
| Example 4     | `tests/agent/test_real_time_injection.cpp`                                  |
| Example 5     | `tests/agent/test_pattern_a_delegation.cpp`, `examples/cli_driver.cpp`      |
| Example 6     | `tests/agent/test_pattern_b_messaging.cpp`                                  |
| Example 7     | `tests/agent/test_pattern_c_blackboard.cpp`                                 |
| Example 8     | `tests/agent/test_fan_out_in.cpp`                                           |
| Example 9     | `tests/agent/test_agent_loop.cpp`                                           |
| Example 10    | `tests/agent/test_batch_executor.cpp`, `tests/agent/test_determinism.cpp`   |
| Example 11    | `tests/agent/test_cycle_detection.cpp`                                      |
| Example 12    | `tests/agent/test_concurrency.cpp`                                          |
| Example 13    | `tests/agent/test_starvation.cpp`                                           |
| Example 14    | `tests/agent/test_cancellation.cpp`                                         |

---

## 13. Known Limitations

| Item | Status |
|------|--------|
| `am_fan_out` / `am_fan_out_free_array` C ABI | `AgentFuture**` array allocation/free semantics may differ by implementation |
| `MCP tool registration` | Server connection stored; actual tool `registerItem()` calls are a TODO in `connectMCP()` |
| `PromptLoader` placeholder detection | Throws on *any* unresolved `{{KEY}}` — multi-stage templates sharing a file must include all placeholders |
| `max_agent_depth` enforcement | Tracked in `AgentConfig::current_depth`; enforced by SpawnChildAction caller convention, not engine-level guard |
| `ran_in_parallel` flag accuracy | Set to `true` when the ready-set has >1 item; may be `false` for the first item of a large batch if it completes before others are submitted |
