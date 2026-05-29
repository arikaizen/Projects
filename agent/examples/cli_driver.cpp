// cli_driver.cpp — Demonstrates Pattern A piping + parallel batch via C ABI.
//
// Requires no real LLM access — all agents run with the mock LLM (empty plan),
// so they terminate immediately after the injected actions run.
//
// Demonstrates:
//   1. am_create / am_destroy lifecycle
//   2. Pattern A: researcher → writer via am_pipe
//   3. Parallel batch: 3 concurrent EchoActions via am_blackboard_write/read
//   4. Buffer-too-small error handling
//   5. am_reload_prompts hot-reload
//   6. am_set_user_quota + quota exceeded check
//
// Build against libagent_engine.so and include agent_engine/c_api.h.

#include "agent_engine/c_api.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

static char gbuf[65536];
#define BUF gbuf, sizeof(gbuf)

static void check(am_status_t s, const char* ctx, AgentManager* mgr = nullptr) {
    if (s != AM_OK) {
        fprintf(stderr, "[FAIL] %s: status=%d  (%s)\n",
                ctx, (int)s, am_last_error(mgr));
    } else {
        fprintf(stdout, "[OK]   %s\n", ctx);
    }
}

static void write_stub_prompts(const char* dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
    system(cmd);

    FILE* f;
    char path[512];

    snprintf(path, sizeof(path), "%s/reason_stage.md", dir);
    f = fopen(path, "w");
    fputs("CATALOG:{{CATALOG}}\nHISTORY:{{HISTORY}}\nQUEUE:{{QUEUE}}\n"
          "TASK:{{TASK}}\nSCHEMA:{{OUTPUT_SCHEMA}}\n", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/injection_stage.md", dir);
    f = fopen(path, "w");
    fputs("CATALOG:{{CATALOG}}\nHISTORY:{{HISTORY}}\nQUEUE:{{QUEUE}}\n"
          "TASK:{{TASK}}\nPREVIOUS:{{PREVIOUS_RESULT}}\nSCHEMA:{{OUTPUT_SCHEMA}}\n", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/transform_stage.md", dir);
    f = fopen(path, "w");
    fputs("INSTRUCTION:{{INSTRUCTION}}\nINPUT:{{INPUT_TEXT}}\n", f);
    fclose(f);

    snprintf(path, sizeof(path), "%s/validate_stage.md", dir);
    f = fopen(path, "w");
    fputs("TARGET:{{TARGET_OUTPUT}}\nCRITERIA:{{CRITERIA}}\n", f);
    fclose(f);
}

int main() {
    fprintf(stdout, "=== Agent Engine CLI Driver ===\n");
    fprintf(stdout, "API version: %d\n\n", am_api_version());

    const char* PROMPTS_DIR = "/tmp/cli_driver_prompts";
    write_stub_prompts(PROMPTS_DIR);

    // ── 1. Create manager ─────────────────────────────────────────────────────
    char config[512];
    snprintf(config, sizeof(config),
             "{\"prompts_dir\":\"%s\",\"thread_pool_size\":4}", PROMPTS_DIR);

    AgentManager* mgr = am_create(config);
    if (!mgr) {
        fprintf(stderr, "am_create failed: %s\n", am_last_error(nullptr));
        return 1;
    }
    fprintf(stdout, "[OK]   am_create\n");

    // ── 2. Demo: Pattern A — researcher → writer pipe ─────────────────────────
    fprintf(stdout, "\n--- Demo 1: Pattern A (researcher -> writer) ---\n");

    char researcher_id[256] = {}, writer_id[256] = {};

    check(am_spawn_agent(mgr,
        "{\"name\":\"researcher\",\"max_iterations\":5}",
        researcher_id, sizeof(researcher_id)), "spawn researcher", mgr);

    check(am_spawn_agent(mgr,
        "{\"name\":\"writer\",\"max_iterations\":5}",
        writer_id, sizeof(writer_id)), "spawn writer", mgr);

    if (strlen(researcher_id) > 0 && strlen(writer_id) > 0) {
        check(am_pipe(mgr, researcher_id, writer_id,
                      "Based on this research: {{OUTPUT}}, write a summary."),
              "am_pipe", mgr);

        // Run researcher — with mock LLM it terminates immediately
        AgentFuture* rfut = am_run_agent(mgr, researcher_id,
                                         "{\"task\":\"Investigate the EU AI Act\"}");
        if (rfut) {
            am_status_t ws = am_future_wait(rfut, 5000, BUF);
            fprintf(stdout, "[OK]   researcher finished (status=%d)\n", (int)ws);
            if (ws == AM_OK && gbuf[0])
                fprintf(stdout, "       result: %.200s\n", gbuf);
            am_future_free(rfut);
        } else {
            fprintf(stderr, "[FAIL] am_run_agent(researcher): %s\n",
                    am_last_error(mgr));
        }

        // Writer's inbox should have the pipe message
        gbuf[0] = '\0';
        am_status_t di = am_drain_inbox(mgr, writer_id, BUF);
        if (di == AM_OK)
            fprintf(stdout, "[OK]   writer inbox: %.200s\n", gbuf[0] ? gbuf : "[]");

        am_destroy_agent(mgr, researcher_id);
        am_destroy_agent(mgr, writer_id);
    }

    // ── 3. Demo: Blackboard write / read ─────────────────────────────────────
    fprintf(stdout, "\n--- Demo 2: Blackboard write/read ---\n");

    check(am_blackboard_write(mgr, "findings/demo",
                              "{\"text\":\"hello from CLI\"}"),
          "blackboard_write", mgr);

    gbuf[0] = '\0';
    am_status_t rs = am_blackboard_read(mgr, "findings/demo", BUF);
    check(rs, "blackboard_read", mgr);
    if (rs == AM_OK)
        fprintf(stdout, "       value: %s\n", gbuf);

    // ── 4. Demo: Buffer-too-small ─────────────────────────────────────────────
    fprintf(stdout, "\n--- Demo 3: Buffer-too-small handling ---\n");

    char tiny[4] = {};
    am_status_t ts = am_blackboard_read(mgr, "findings/demo", tiny, sizeof(tiny));
    fprintf(stdout, "[%s]   small-buffer status=%d (expected %d = AM_ERROR_BUFFER_TOO_SMALL)\n",
            ts == AM_ERROR_BUFFER_TOO_SMALL ? "OK" : "NOTE",
            (int)ts, (int)AM_ERROR_BUFFER_TOO_SMALL);

    // ── 5. Demo: Hot reload ───────────────────────────────────────────────────
    fprintf(stdout, "\n--- Demo 4: Hot reload ---\n");
    check(am_reload_prompts(mgr), "am_reload_prompts", mgr);

    // ── 6. Demo: Quota enforcement ────────────────────────────────────────────
    fprintf(stdout, "\n--- Demo 5: Quota enforcement ---\n");

    check(am_set_user_quota(mgr, "demo_user",
                            "{\"max_concurrent_agents\":2}"),
          "am_set_user_quota", mgr);

    char qid1[256]={}, qid2[256]={}, qid3[256]={};
    am_spawn_agent(mgr, "{\"user_id\":\"demo_user\",\"name\":\"Q1\"}",
                   qid1, sizeof(qid1));
    am_spawn_agent(mgr, "{\"user_id\":\"demo_user\",\"name\":\"Q2\"}",
                   qid2, sizeof(qid2));

    am_status_t quota_st = am_spawn_agent(
        mgr, "{\"user_id\":\"demo_user\",\"name\":\"Q3\"}", qid3, sizeof(qid3));
    fprintf(stdout, "[%s]   3rd spawn status=%d (expected %d = AM_ERROR_QUOTA_EXCEEDED)\n",
            quota_st == AM_ERROR_QUOTA_EXCEEDED ? "OK" : "NOTE",
            (int)quota_st, (int)AM_ERROR_QUOTA_EXCEEDED);

    if (strlen(qid1) > 0) am_destroy_agent(mgr, qid1);
    if (strlen(qid2) > 0) am_destroy_agent(mgr, qid2);

    // ── 7. Demo: Messaging (Pattern B) ───────────────────────────────────────
    fprintf(stdout, "\n--- Demo 6: Pattern B messaging ---\n");

    char from_id[256]={}, to_id[256]={};
    am_spawn_agent(mgr, "{\"name\":\"coordinator\"}", from_id, sizeof(from_id));
    am_spawn_agent(mgr, "{\"name\":\"worker\"}",      to_id,   sizeof(to_id));

    if (strlen(from_id) > 0 && strlen(to_id) > 0) {
        check(am_send_message(mgr, from_id, to_id,
                              "{\"type\":\"task\",\"payload\":\"process data\"}"),
              "am_send_message", mgr);

        gbuf[0] = '\0';
        am_status_t di2 = am_drain_inbox(mgr, to_id, BUF);
        check(di2, "am_drain_inbox", mgr);
        if (di2 == AM_OK)
            fprintf(stdout, "       messages: %.200s\n", gbuf);

        am_destroy_agent(mgr, from_id);
        am_destroy_agent(mgr, to_id);
    }

    // ── 8. Cleanup ────────────────────────────────────────────────────────────
    am_destroy(mgr);
    fprintf(stdout, "\n[OK]   am_destroy — done.\n");
    return 0;
}
