#pragma once

#include "SavePolicy.hpp"
#include "IEvent.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

/**
 * Scheduler
 * ──────────────────────────────────────────────────────────────────────────
 * Owns a list of IEvent instances and decides when to fire them based on a
 * SavePolicy.
 *
 * One Scheduler lives per conversation (inside ConvoEntry).
 *
 * Typical setup
 * ─────────────
 *   Scheduler s;
 *   s.Register(std::make_unique<SaveHistoryEvent>(convo, outDir));
 *   s.Register(std::make_unique<SaveTokensEvent>(convo, outDir));
 *   s.Register(std::make_unique<SaveKvCacheEvent>(convo, outDir));
 *   s.Register(std::make_unique<SaveMetaEvent>(convo, modelPath, ctxSize, outDir));
 *
 * Inserting a saving schedule
 * ───────────────────────────
 * 1. After every Chat() call, pass the current state to Run():
 *      scheduler.Run(turn_count, tokens_processed, ctx_size);
 *    The scheduler checks all three policy triggers and fires the
 *    appropriate events automatically.
 *
 * 2. To save a specific file on demand (e.g. from a /save-event command):
 *      scheduler.RunByName("SaveHistory");
 *      scheduler.RunByName("SaveTokens");
 *      scheduler.RunByName("SaveKvCache");
 *      scheduler.RunByName("SaveMeta");
 *
 * 3. To save everything immediately (e.g. on /close or SIGINT):
 *      scheduler.RunAll();
 *
 * 4. To change the policy at runtime (e.g. from a /schedule command):
 *      SavePolicy p;
 *      p.TurnInterval = 10;
 *      scheduler.SetPolicy(p);
 */
class Scheduler {
public:
    explicit Scheduler(SavePolicy policy = {});

    // ── Registration ──────────────────────────────────────────────────────────

    /** Add an event to the scheduler.  Ownership is transferred. */
    void Register(std::unique_ptr<IEvent> event);

    // ── Automatic trigger (call after every Chat()) ───────────────────────────

    /**
     * Check SavePolicy triggers and fire events as needed.
     *
     * @param total_turns      Total chat turns completed so far.
     * @param tokens_processed Tokens currently in the KV cache.
     * @param ctx_size         Maximum context size in tokens.
     *
     * Trigger rules:
     *   ctx_threshold hit  → RunAll()
     *   turn_interval hit  → RunByName("SaveHistory")
     *   idle_seconds hit   → RunByName("SaveHistory")
     */
    void Run(int total_turns, int tokens_processed, int ctx_size);

    // ── Manual triggers ───────────────────────────────────────────────────────

    /** Fire every enabled event. */
    void RunAll();

    /** Fire the single event whose Name() matches.  No-op if not found. */
    void RunByName(const std::string& name);

    // ── Policy ────────────────────────────────────────────────────────────────

    const SavePolicy& Policy() const noexcept { return _policy; }
    void SetPolicy(const SavePolicy& policy)  { _policy = policy; }

private:
    void ResetCounters();

    SavePolicy   _policy;
    std::vector<std::unique_ptr<IEvent>> _events;

    int  _turns_since_save = 0;
    std::chrono::steady_clock::time_point _last_save_time;
};
