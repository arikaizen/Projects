// batch_executor.cpp — Dependency-aware parallel execution of a WorkItem batch.
//
// Thread-safety design:
//   - The executor's main loop (while done_count < n) runs on the AGENT LOOP
//     THREAD.  Only this thread writes to `results`, `node_done`, `ready`, and
//     `in_flight`.
//   - Pool workers execute items concurrently.  Each worker receives its item
//     and pre-resolved inputs before being submitted; it does not write shared
//     state except through the WorkItem's execute() method (which may call
//     ctx.push() — thread-safe — and ctx.resolveReferences() — which reads
//     ctx.history under a mutex, safe since the agent thread is the only
//     writer of history, and that write happens AFTER the batch completes).
//
// Reference resolution:
//   Items may reference $other_id outputs.  For items already in ctx.history
//   (pre-batch), resolution works normally.  For items within the same batch,
//   resolution uses `results[]` indexed by batch position.  The happens-before
//   guarantee: the agent thread writes results[A_idx] BEFORE submitting item B
//   (which depends on A) to the pool.  The pool's submit() uses a mutex,
//   creating a happens-before relationship that makes results[A_idx] visible
//   to B's worker thread.
//
// Deterministic merge (req 35):
//   `ctx.recordResult` is called ONLY ONCE, in declared order, in the final
//   merge loop after all items are done — never by multiple threads at once.
#include "agent/batch_executor.hpp"
#include "agent/agent_context.hpp"
#include "agent/event_bus.hpp"
#include "agent/work_item.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>
#include <thread>

namespace agent {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
BatchExecutor::BatchExecutor(ThreadPool& pool)
    : m_pool(pool)
{}

// ---------------------------------------------------------------------------
// resolveInBatch — resolve $refs against history AND within-batch results
//
// Called on the AGENT THREAD before submitting each item to the pool.
// After resolution, inputs are concrete values with no $ref strings.
// Pool workers receive pre-resolved inputs and do not race on this.
// ---------------------------------------------------------------------------
static nlohmann::json resolveInBatch(
    const nlohmann::json&                           inputs,
    const AgentContext&                             ctx,
    const std::vector<WorkResult>&                  batch_results,
    const std::map<std::string, std::size_t>&       id_to_idx,
    const std::vector<bool>&                        node_done)
{
    static const std::regex ref_re(R"(^\$([A-Za-z0-9_]+)(\.([A-Za-z0-9_]+))?$)");

    std::function<nlohmann::json(const nlohmann::json&)> walk =
        [&](const nlohmann::json& j) -> nlohmann::json {
            if (j.is_string()) {
                std::string s = j.get<std::string>();
                std::smatch m;
                if (std::regex_match(s, m, ref_re)) {
                    std::string item_id  = m[1].str();
                    std::string field    = m[3].str();

                    // Check pre-batch history first
                    const WorkResult* hr = ctx.resultById(item_id);
                    if (hr) {
                        return field.empty() ? hr->output : hr->output.value(field, nlohmann::json{});
                    }

                    // Check within-batch completed results
                    auto it = id_to_idx.find(item_id);
                    if (it != id_to_idx.end()) {
                        std::size_t idx = it->second;
                        if (node_done[idx]) {
                            const auto& r = batch_results[idx];
                            return field.empty() ? r.output : r.output.value(field, nlohmann::json{});
                        }
                    }

                    throw std::runtime_error(
                        "resolveInBatch: unresolvable reference '" + s + "'");
                }
                return j;
            }
            if (j.is_array()) {
                nlohmann::json out = nlohmann::json::array();
                for (const auto& el : j) out.push_back(walk(el));
                return out;
            }
            if (j.is_object()) {
                nlohmann::json out = nlohmann::json::object();
                for (auto it = j.begin(); it != j.end(); ++it)
                    out[it.key()] = walk(it.value());
                return out;
            }
            return j;
        };
    return walk(inputs);
}

// ---------------------------------------------------------------------------
// buildDAG
// ---------------------------------------------------------------------------
bool BatchExecutor::buildDAG(const std::vector<std::unique_ptr<WorkItem>>& batch,
                              const AgentContext&                            ctx,
                              std::vector<Node>&                             nodes,
                              std::string&                                   error_out)
{
    const std::size_t n = batch.size();

    std::map<std::string, std::size_t> id_to_idx;
    for (std::size_t i = 0; i < n; ++i) {
        id_to_idx[batch[i]->id] = i;
        nodes[i].item = batch[i].get();
    }

    for (std::size_t i = 0; i < n; ++i) {
        auto deps = batch[i]->dependencies();
        for (const auto& dep_id : deps) {
            if (ctx.resultById(dep_id) != nullptr)
                continue;  // satisfied by pre-batch history

            auto it = id_to_idx.find(dep_id);
            if (it != id_to_idx.end()) {
                std::size_t dep_idx = it->second;
                nodes[dep_idx].dependents.push_back(i);
                nodes[i].pending_deps.insert(dep_id);
                continue;
            }

            error_out = "Dependency '" + dep_id + "' required by '" +
                        batch[i]->id + "' is neither in history nor in this batch";
            return false;
        }
    }

    // Kahn's cycle detection
    std::vector<std::size_t> in_degree(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        in_degree[i] = nodes[i].pending_deps.size();

    std::queue<std::size_t> ready_q;
    for (std::size_t i = 0; i < n; ++i)
        if (in_degree[i] == 0) ready_q.push(i);

    std::size_t processed = 0;
    while (!ready_q.empty()) {
        std::size_t cur = ready_q.front(); ready_q.pop();
        ++processed;
        for (std::size_t dep_idx : nodes[cur].dependents)
            if (--in_degree[dep_idx] == 0) ready_q.push(dep_idx);
    }

    if (processed < n) {
        error_out = "Cycle detected in batch (" +
                    std::to_string(processed) + "/" + std::to_string(n) + " nodes acyclic)";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// execute
// ---------------------------------------------------------------------------
std::vector<WorkResult> BatchExecutor::execute(
    std::vector<std::unique_ptr<WorkItem>> batch,
    AgentContext&                          ctx)
{
    if (batch.empty()) return {};

    const std::size_t n = batch.size();

    // Build id → batch index (needed both for DAG and in-batch resolution)
    std::map<std::string, std::size_t> id_to_idx;
    for (std::size_t i = 0; i < n; ++i)
        id_to_idx[batch[i]->id] = i;

    // Build DAG
    std::vector<Node> nodes(n);
    std::string       dag_error;
    if (!buildDAG(batch, ctx, nodes, dag_error))
        throw std::runtime_error("Dependency cycle in batch: " + dag_error);

    // Declare-order results array — written by agent thread only, read by
    // pool workers ONLY AFTER the agent thread writes results[dep_idx] and
    // BEFORE the pool worker starts (enforced by pool's submit mutex).
    std::vector<WorkResult> results(n);
    std::vector<bool>       node_done(n, false);
    std::size_t             done_count = 0;

    std::map<std::size_t, std::future<WorkResult>> in_flight;

    // Initial ready set
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i)
        if (nodes[i].pending_deps.empty() && !nodes[i].skipped)
            ready.push_back(i);

    // Transitive skip propagation (callable from agent thread only)
    std::function<void(std::size_t, const std::string&)> propagateSkip;
    propagateSkip = [&](std::size_t idx, const std::string& reason) {
        if (node_done[idx]) return;
        nodes[idx].skipped     = true;
        nodes[idx].skip_reason = reason;

        auto& r          = results[idx];
        r.item_id        = batch[idx]->id;
        r.item_name      = batch[idx]->name;
        r.item_kind      = (batch[idx]->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
        r.success        = false;
        r.skipped_reason = reason;
        r.error          = reason;
        r.timestamp      = std::chrono::system_clock::now();

        node_done[idx] = true;
        ++done_count;

        for (std::size_t dep_idx : nodes[idx].dependents) {
            nodes[dep_idx].pending_deps.erase(batch[idx]->id);
            if (!node_done[dep_idx])
                propagateSkip(dep_idx, "Dependency '" + batch[idx]->id + "' was skipped/failed");
        }
    };

    for (std::size_t i = 0; i < n; ++i)
        if (nodes[i].skipped && !node_done[i])
            propagateSkip(i, nodes[i].skip_reason.empty() ? "Pre-skipped" : nodes[i].skip_reason);

    // Execute a single item (inputs already resolved on the agent thread).
    // Used both by pool workers and the inline fast-path below.
    auto runItem = [&ctx](WorkItem* raw_item, bool parallel) -> WorkResult {
        auto start = std::chrono::steady_clock::now();
        WorkResult r;
        r.item_id         = raw_item->id;
        r.item_name       = raw_item->name;
        r.item_kind       = (raw_item->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
        r.ran_in_parallel = parallel;
        try {
            r = raw_item->execute(ctx);
        } catch (const std::exception& ex) {
            r.success = false;
            r.error   = ex.what();
        } catch (...) {
            r.success = false;
            r.error   = "Unknown exception during execution";
        }
        r.item_id         = raw_item->id;
        r.item_name       = raw_item->name;
        r.item_kind       = (raw_item->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
        r.ran_in_parallel = parallel;
        r.duration        = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start);
        r.timestamp       = std::chrono::system_clock::now();
        return r;
    };

    // Apply a completed result: record dependents that just became ready, or
    // propagate skips on failure.  Runs on the agent thread only.
    auto applyCompletion = [&](std::size_t idx) {
        bool succeeded = results[idx].success;
        if (ctx.eventBus()) {
            ctx.eventBus()->emit(EventBus::makeEvent("work_item_finished", {
                {"agent_id",        ctx.config().agent_id},
                {"item_id",         results[idx].item_id},
                {"success",         results[idx].success},
                {"duration_ms",     static_cast<int>(results[idx].duration.count())},
                {"ran_in_parallel", results[idx].ran_in_parallel}
            }));
        }
        for (std::size_t dep_idx : nodes[idx].dependents) {
            nodes[dep_idx].pending_deps.erase(batch[idx]->id);
            if (!succeeded) {
                if (!node_done[dep_idx])
                    propagateSkip(dep_idx, "Dependency '" + batch[idx]->id + "' failed");
            } else if (nodes[dep_idx].pending_deps.empty() &&
                       !nodes[dep_idx].skipped &&
                       !node_done[dep_idx]) {
                ready.push_back(dep_idx);
            }
        }
    };

    while (done_count < n) {
        // ── Cancellation check ───────────────────────────────────────────────
        if (ctx.cancellation_flag.load()) {
            for (std::size_t i = 0; i < n; ++i) {
                if (!node_done[i]) {
                    results[i].item_id       = batch[i]->id;
                    results[i].item_name     = batch[i]->name;
                    results[i].item_kind     = (batch[i]->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
                    results[i].success       = false;
                    results[i].error         = "Cancelled";
                    results[i].skipped_reason = "Cancelled";
                    results[i].timestamp     = std::chrono::system_clock::now();
                    node_done[i] = true;
                    ++done_count;
                }
            }
            break;
        }

        // ── Resolve inputs on the AGENT THREAD before dispatch ───────────────
        // This guarantees results[dep_idx] is already written (and visible via
        // the pool's submit mutex) when a worker reads it.  Resolution failures
        // fail the item immediately and propagate to dependents.
        std::vector<std::size_t> dispatch;
        for (std::size_t idx : ready) {
            if (node_done[idx]) continue;
            try {
                batch[idx]->inputs = resolveInBatch(
                    batch[idx]->inputs, ctx, results, id_to_idx, node_done);
                dispatch.push_back(idx);
            } catch (const std::exception& ex) {
                results[idx].item_id    = batch[idx]->id;
                results[idx].item_name  = batch[idx]->name;
                results[idx].item_kind  = (batch[idx]->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
                results[idx].success    = false;
                results[idx].error      = std::string("ref resolution: ") + ex.what();
                results[idx].timestamp  = std::chrono::system_clock::now();
                node_done[idx] = true;
                ++done_count;
                propagateSkip(idx, results[idx].error);
            }
        }
        ready.clear();

        // ── Dispatch ─────────────────────────────────────────────────────────
        // Starvation avoidance (req 32): when only a single item is ready and
        // nothing is already running, execute it INLINE on the agent thread
        // instead of handing it to a pool worker and busy-polling.  The agent
        // loop itself runs on a pool thread, so inline execution reuses that
        // thread for real work — nested sub-agents therefore consume one pool
        // thread per level instead of two, and a small pool never deadlocks.
        // When several items are ready we submit all-but-one to the pool and run
        // the last inline, preserving true parallelism (Level 3 concurrency).
        if (!dispatch.empty()) {
            bool parallel = (dispatch.size() + in_flight.size()) > 1;
            std::size_t inline_idx = dispatch.back();

            for (std::size_t k = 0; k + 1 < dispatch.size(); ++k) {
                std::size_t idx      = dispatch[k];
                WorkItem*   raw_item = batch[idx].get();
                if (ctx.eventBus()) {
                    ctx.eventBus()->emit(EventBus::makeEvent("work_item_started", {
                        {"agent_id",        ctx.config().agent_id},
                        {"item_id",         raw_item->id},
                        {"item_name",       raw_item->name},
                        {"ran_in_parallel", true}
                    }));
                }
                in_flight[idx] = m_pool.submit(
                    [&runItem, raw_item]() -> WorkResult { return runItem(raw_item, true); });
            }

            // Run the chosen item inline on this thread.
            {
                WorkItem* raw_item = batch[inline_idx].get();
                if (ctx.eventBus()) {
                    ctx.eventBus()->emit(EventBus::makeEvent("work_item_started", {
                        {"agent_id",        ctx.config().agent_id},
                        {"item_id",         raw_item->id},
                        {"item_name",       raw_item->name},
                        {"ran_in_parallel", parallel}
                    }));
                }
                results[inline_idx] = runItem(raw_item, parallel);
                node_done[inline_idx] = true;
                ++done_count;
                applyCompletion(inline_idx);
            }
            // More work may now be ready (dependents of the inline item) — loop.
            continue;
        }

        // Guard: if nothing is in-flight and we're not done, something went wrong
        if (in_flight.empty() && done_count < n) {
            std::cerr << "[BatchExecutor] WARNING: stalled with "
                      << (n - done_count) << " items remaining\n";
            for (std::size_t i = 0; i < n; ++i) {
                if (!node_done[i]) {
                    results[i].item_id    = batch[i]->id;
                    results[i].item_name  = batch[i]->name;
                    results[i].item_kind  = (batch[i]->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
                    results[i].success    = false;
                    results[i].error      = "Scheduler stall (DAG invariant violated)";
                    results[i].timestamp  = std::chrono::system_clock::now();
                    node_done[i] = true;
                    ++done_count;
                }
            }
            break;
        }

        // ── Poll for completed futures ───────────────────────────────────────
        std::vector<std::size_t> just_completed;
        while (just_completed.empty()) {
            for (auto& [idx, fut] : in_flight) {
                if (fut.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready)
                    just_completed.push_back(idx);
            }
            if (just_completed.empty())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // ── Collect and propagate ────────────────────────────────────────────
        for (std::size_t idx : just_completed) {
            try {
                results[idx] = in_flight.at(idx).get();
            } catch (const std::exception& ex) {
                results[idx].item_id    = batch[idx]->id;
                results[idx].item_name  = batch[idx]->name;
                results[idx].item_kind  = (batch[idx]->kind() == WorkItem::Kind::Stage) ? "Stage" : "Action";
                results[idx].success    = false;
                results[idx].error      = ex.what();
                results[idx].timestamp  = std::chrono::system_clock::now();
            }
            in_flight.erase(idx);
            node_done[idx] = true;
            ++done_count;
            applyCompletion(idx);
        }
    }

    // ── Deterministic merge (req 35): fold results into ctx.history in
    //    declared order at a single synchronization point on the agent thread.
    for (auto& r : results) {
        if (!r.item_id.empty())
            ctx.recordResult(r);
    }

    return results;
}

} // namespace agent
