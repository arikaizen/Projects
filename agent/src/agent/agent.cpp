// agent.cpp — Agent run loop
//
// Loop design:
//   1. Blocking pop() of the first available item.
//   2. Execute that item as a 1-item batch via BatchExecutor.
//      If it is a Stage its execute() may push N child items into the queue.
//   3. Non-blocking drain of ALL items now in the queue (try_pop loop).
//      If the stage pushed a plan, those N items are collected here.
//   4. If any items were drained, execute them together as a multi-item batch
//      (BatchExecutor handles dependency-aware parallelism across them).
//   5. Increment iteration_count and repeat.
//
// This approach naturally batches items that a Stage pushes atomically while
// still allowing items injected from outside (injectFromOutside) to join the
// same batch if they arrive before step 3.
#include "agent/agent.hpp"
#include "agent/agent_context.hpp"
#include "agent/batch_executor.hpp"
#include "agent/event_bus.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Agent::Agent(std::unique_ptr<AgentContext> ctx, ThreadPool& pool)
    : m_ctx     (std::move(ctx))
    , m_executor(pool)
{}

// ---------------------------------------------------------------------------
// reasonToString
// ---------------------------------------------------------------------------
std::string Agent::reasonToString(TerminationReason r)
{
    switch (r) {
        case TerminationReason::QueueEmpty:     return "queue_empty";
        case TerminationReason::ShouldStop:     return "should_stop";
        case TerminationReason::MaxIterations:  return "max_iterations";
        case TerminationReason::Cancelled:      return "cancelled";
        case TerminationReason::Error:          return "error";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// run — the main agent loop
// ---------------------------------------------------------------------------
Agent::RunResult Agent::run()
{
    AgentContext& ctx = *m_ctx;
    const std::string agent_id = ctx.config().agent_id;

    std::cerr << "[AGENT] " << agent_id << " starting\n";

    if (ctx.eventBus()) {
        ctx.eventBus()->emit(EventBus::makeEvent("agent_started",
                             {{"agent_id", agent_id}}));
    }

    while (true) {
        // ── Pre-iteration termination checks ────────────────────────────────

        if (ctx.cancellation_flag.load()) {
            std::cerr << "[AGENT] " << agent_id << " cancelled\n";
            if (ctx.eventBus()) {
                ctx.eventBus()->emit(EventBus::makeEvent("agent_cancelled",
                                     {{"agent_id", agent_id}}));
            }
            return RunResult{TerminationReason::Cancelled,
                             ctx.final_output,
                             ctx.iteration_count,
                             "cancelled"};
        }

        if (ctx.should_stop) {
            std::cerr << "[AGENT] " << agent_id << " stop requested\n";
            if (ctx.eventBus()) {
                ctx.eventBus()->emit(EventBus::makeEvent("agent_finished",
                                     {{"agent_id", agent_id},
                                      {"reason", "should_stop"}}));
            }
            return RunResult{TerminationReason::ShouldStop,
                             ctx.final_output,
                             ctx.iteration_count,
                             ""};
        }

        if (ctx.iteration_count >= ctx.config().max_iterations) {
            std::cerr << "[AGENT] " << agent_id << " max_iterations reached\n";
            return RunResult{TerminationReason::MaxIterations,
                             ctx.final_output,
                             ctx.iteration_count,
                             "max_iterations exceeded"};
        }

        // ── Blocking pop of first item ───────────────────────────────────────

        auto first_item = ctx.pop();   // blocks until item or termination signal

        if (!first_item) {
            // Queue empty and agent should stop (cancellation or should_stop
            // was set while waiting).
            if (ctx.cancellation_flag.load()) {
                if (ctx.eventBus()) {
                    ctx.eventBus()->emit(EventBus::makeEvent("agent_cancelled",
                                         {{"agent_id", agent_id}}));
                }
                return RunResult{TerminationReason::Cancelled,
                                 ctx.final_output,
                                 ctx.iteration_count,
                                 "cancelled"};
            }
            // Normal queue-empty termination
            std::cerr << "[AGENT] " << agent_id << " queue empty, finishing\n";
            if (ctx.eventBus()) {
                ctx.eventBus()->emit(EventBus::makeEvent("agent_finished",
                                     {{"agent_id", agent_id},
                                      {"reason", "queue_empty"}}));
            }
            return RunResult{TerminationReason::QueueEmpty,
                             ctx.final_output,
                             ctx.iteration_count,
                             ""};
        }

        // ── Execute first item ───────────────────────────────────────────────
        // Run it as a 1-item batch.  If it is a Stage, its execute() body may
        // call ctx.push() multiple times to enqueue a plan.

        std::vector<std::unique_ptr<WorkItem>> first_batch;
        first_batch.push_back(std::move(first_item));

        if (ctx.eventBus()) {
            ctx.eventBus()->emit(EventBus::makeEvent("batch_started",
                                 {{"agent_id", agent_id},
                                  {"batch_size", 1},
                                  {"phase", "stage_or_single"}}));
        }

        try {
            auto first_results = m_executor.execute(std::move(first_batch), ctx);
            if (ctx.eventBus()) {
                ctx.eventBus()->emit(EventBus::makeEvent("batch_finished",
                                     {{"agent_id",      agent_id},
                                      {"results_count", static_cast<int>(first_results.size())}}));
            }
        } catch (const std::exception& ex) {
            std::cerr << "[AGENT] " << agent_id
                      << " batch error: " << ex.what() << "\n";
            WorkResult err;
            err.success = false;
            err.error   = ex.what();
            ctx.recordResult(err);
        }

        ++ctx.iteration_count;

        // ── Drain any items pushed during the first item's execution ─────────
        // These are typically the plan items emitted by a Stage; running them
        // as a batch enables dependency-aware parallel execution.

        std::vector<std::unique_ptr<WorkItem>> plan_batch;
        while (auto next = ctx.try_pop()) {
            plan_batch.push_back(std::move(next));
        }

        if (!plan_batch.empty()) {
            std::cerr << "[AGENT] " << agent_id
                      << " executing plan batch of " << plan_batch.size()
                      << " items\n";

            if (ctx.eventBus()) {
                ctx.eventBus()->emit(EventBus::makeEvent("batch_started",
                                     {{"agent_id",   agent_id},
                                      {"batch_size", static_cast<int>(plan_batch.size())},
                                      {"phase", "plan"}}));
            }

            try {
                auto plan_results = m_executor.execute(std::move(plan_batch), ctx);
                if (ctx.eventBus()) {
                    ctx.eventBus()->emit(EventBus::makeEvent("batch_finished",
                                         {{"agent_id",      agent_id},
                                          {"results_count", static_cast<int>(plan_results.size())}}));
                }
            } catch (const std::exception& ex) {
                std::cerr << "[AGENT] " << agent_id
                          << " plan batch error: " << ex.what() << "\n";
                WorkResult err;
                err.success = false;
                err.error   = ex.what();
                ctx.recordResult(err);
            }
        }

        // ── Post-iteration checks (a stage may have set should_stop) ─────────
        // Loop back to top where all checks are evaluated.
    }
}

} // namespace agent
