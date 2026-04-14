#include "Scheduler.hpp"

Scheduler::Scheduler(SavePolicy policy)
    : _policy(policy)
    , _last_save_time(std::chrono::steady_clock::now())
{}

void Scheduler::Register(std::unique_ptr<IEvent> event) {
    if (event) _events.push_back(std::move(event));
}

void Scheduler::Run(int /*total_turns*/, int tokens_processed, int ctx_size) {
    ++_turns_since_save;

    const auto now     = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                             now - _last_save_time).count();

    const float ctx_usage = (ctx_size > 0)
        ? static_cast<float>(tokens_processed) / static_cast<float>(ctx_size)
        : 0.0f;

    const bool ctx_hit  = (_policy.CtxThreshold  > 0.0f)
                       && (ctx_usage             >= _policy.CtxThreshold);
    const bool turn_hit = (_policy.TurnInterval  >  0)
                       && (_turns_since_save     >= _policy.TurnInterval);
    const bool idle_hit = (_policy.IdleSeconds   >  0)
                       && (elapsed              >= static_cast<long long>(_policy.IdleSeconds));

    if (ctx_hit) {
        // Context near full — save everything so the session can be resumed
        // without re-decoding the entire history.
        RunAll();
    } else if (turn_hit || idle_hit) {
        // Routine save — history only (cheap).
        RunByName("SaveHistory");
    }
}

void Scheduler::RunAll() {
    for (auto& ev : _events) {
        if (!ev || !ev->Enabled) continue;
        try {
            ev->Run();
            ev->LastRun = std::chrono::steady_clock::now();
        } catch (...) {
            // best-effort: one failing event must not block the others
        }
    }
    ResetCounters();
}

void Scheduler::RunByName(const std::string& name) {
    for (auto& ev : _events) {
        if (!ev || !ev->Enabled) continue;
        if (ev->Name() != name) continue;
        try {
            ev->Run();
            ev->LastRun = std::chrono::steady_clock::now();
        } catch (...) {}
        ResetCounters();
        return;
    }
}

void Scheduler::ResetCounters() {
    _turns_since_save = 0;
    _last_save_time   = std::chrono::steady_clock::now();
}
