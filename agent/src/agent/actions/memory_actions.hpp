#pragma once
#include "agent/action.hpp"
#include "agent/work_factory.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace agent {

// ── MemoryWriteAction ────────────────────────────────────────────────────────
// Persists a string entry to the agent's MemoryBackend.
// Thread-safe: MemoryBackend implementations must provide their own
// synchronization (the contract from memory_backend.hpp).
class MemoryWriteAction : public Action {
public:
    MemoryWriteAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// ── MemoryReadAction ─────────────────────────────────────────────────────────
// Performs a semantic (or keyword) search over the MemoryBackend.
class MemoryReadAction : public Action {
public:
    MemoryReadAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// ── MemoryListAction ─────────────────────────────────────────────────────────
// Lists all memory entries, optionally filtered by a string prefix/substring.
class MemoryListAction : public Action {
public:
    MemoryListAction(std::string id, std::string name, nlohmann::json inputs = {})
        : Action(std::move(id), std::move(name), std::move(inputs)) {}

    WorkResult execute(AgentContext& ctx) override;
};

// Registers MemoryWriteAction, MemoryReadAction, and MemoryListAction.
void registerMemoryActions(WorkFactory& factory);

} // namespace agent
