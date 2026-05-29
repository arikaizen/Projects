#pragma once
#include "agent_context.hpp"
#include "batch_executor.hpp"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace agent {

// Owns an AgentContext and runs the agent loop.
// One Agent per task; runs on a ThreadPool worker thread.
//
// Loop invariant:
//   - Pop the next batch from the queue (blocks until one arrives).
//   - Execute the batch via BatchExecutor (dependency-aware parallelism).
//   - Record results into history (deterministic merge).
//   - Repeat until a termination condition is met.
//
// Reasoning (Stage) steps run sequentially within an agent so each sees the
// full prior history; only Action steps are parallelised across a batch.
class Agent {
public:
    enum class TerminationReason {
        QueueEmpty,
        ShouldStop,
        MaxIterations,
        Cancelled,
        Error
    };

    struct RunResult {
        TerminationReason reason;
        nlohmann::json    output;
        int               iterations{0};
        std::string       error;
    };

    explicit Agent(std::unique_ptr<AgentContext> ctx, ThreadPool& pool);
    ~Agent() = default;

    Agent(const Agent&)            = delete;
    Agent& operator=(const Agent&) = delete;

    // Blocking: runs the loop to completion on the calling thread.
    RunResult run();

    AgentContext&       context()       { return *m_ctx; }
    const AgentContext& context() const { return *m_ctx; }

    static std::string reasonToString(TerminationReason r);

private:
    std::unique_ptr<AgentContext> m_ctx;
    BatchExecutor                 m_executor;
};

} // namespace agent
