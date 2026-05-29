#pragma once
#include "work_item.hpp"
#include "thread_pool.hpp"
#include <memory>
#include <vector>

namespace agent {

class AgentContext;

// Executes a batch of WorkItems with dependency-aware parallelism.
//
// Algorithm (Level 3 concurrency):
//   1. Build a DAG: edge A→B when B references A's output (via $A or $A.field).
//      References to ids already in history are satisfied immediately (no edge).
//   2. Verify acyclicity (Kahn's algorithm). Reject the batch if a cycle exists.
//   3. Run: items with no pending dependencies are "ready". All ready items are
//      submitted to the thread pool concurrently.  As each completes, its result
//      is collected; dependents whose last pending dep just finished become ready.
//      Repeat until nothing is left.
//   4. Failure semantics: a failed item's transitive dependents are not started;
//      they are recorded as skipped with a reason. Independent items continue.
//   5. Cancellation: check ctx.cancellation_flag before starting each item;
//      once set, no new items are started.
//   6. Deterministic merge: results are folded into ctx.history in the batch's
//      *declared order* at a single synchronisation point after the batch
//      completes — never by parallel workers mutating history concurrently.
//
// Thread-safety: BatchExecutor is created fresh per batch; it is not shared
// across threads.  It schedules work onto the shared ThreadPool.
class BatchExecutor {
public:
    explicit BatchExecutor(ThreadPool& pool);

    // Execute the batch and record results into ctx.  Returns the list of
    // results in the batch's declared order.  Throws std::runtime_error if the
    // dependency graph contains a cycle (should be caught by plan validation).
    std::vector<WorkResult> execute(
        std::vector<std::unique_ptr<WorkItem>> batch,
        AgentContext& ctx);

private:
    struct Node {
        WorkItem*              item{nullptr};
        std::set<std::string>  pending_deps;   // in-batch deps not yet done
        std::vector<size_t>    dependents;      // indices of nodes that depend on this
        bool                   done{false};
        bool                   skipped{false};
        std::string            skip_reason;
    };

    // Build nodes[] from the batch, resolving which deps are in-history vs in-batch.
    // Returns false (and sets error_out) if a cycle is detected.
    bool buildDAG(const std::vector<std::unique_ptr<WorkItem>>& batch,
                  const AgentContext& ctx,
                  std::vector<Node>& nodes,
                  std::string& error_out);

    ThreadPool& m_pool;
};

} // namespace agent
