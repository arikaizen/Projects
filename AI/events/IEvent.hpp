#pragma once

#include <chrono>
#include <string>

/**
 * IEvent
 * ──────────────────────────────────────────────────────────────────────────
 * Abstract base for all scheduler-managed save events.
 *
 * Each concrete subclass is responsible for exactly one output file:
 *
 *   SaveHistoryEvent  →  history.json
 *   SaveTokensEvent   →  tokens.bin
 *   SaveKvCacheEvent  →  kv_cache.bin
 *   SaveMetaEvent     →  meta.json
 *
 * Subclasses must implement:
 *   Run()   — perform the save operation
 *   Name()  — return the unique event name (used by Scheduler::RunByName)
 */
class IEvent {
public:
    virtual ~IEvent() = default;

    /** Perform the save operation.  Must not throw — swallow and log internally. */
    virtual void Run() = 0;

    /** Unique name used by Scheduler::RunByName(). */
    virtual std::string Name() const = 0;

    /** Set to false to temporarily disable this event without removing it. */
    bool Enabled = true;

    /** Timestamp of the last successful Run() call. */
    std::chrono::steady_clock::time_point LastRun{};
};
