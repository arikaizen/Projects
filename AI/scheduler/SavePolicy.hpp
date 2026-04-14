#pragma once

/**
 * SavePolicy
 * ──────────────────────────────────────────────────────────────────────────
 * Configures when the Scheduler fires save events.
 *
 * Three independent triggers, each targeting a different tier:
 *
 *   TurnInterval   — fire SaveHistory every N chat turns
 *   IdleSeconds    — fire SaveHistory if N seconds pass since last save
 *   CtxThreshold   — fire all events when KV cache is this fraction full
 *
 * Set any value to 0 / 0.0f to disable that trigger.
 */
struct SavePolicy {
    // Save history.json every N chat turns.  0 = disabled.
    int TurnInterval = 5;

    // Save history.json if this many seconds have elapsed since the last
    // save of any kind.  0 = disabled.
    int IdleSeconds = 300;

    // Fire all events (history + tokens + kv_cache + meta) when
    // tokens_processed / ctx_size >= this value.  0.0f = disabled.
    float CtxThreshold = 0.75f;
};
