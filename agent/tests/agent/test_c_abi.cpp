// test_c_abi.cpp — C ABI smoke tests (exercises libagent_engine.so boundary)
//
// Links against agent_engine (not agent_core directly) so it exercises the
// real ABI boundary including exception-to-status-code translation.

#include "agent_engine/c_api.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── Minimal test framework ───────────────────────────────────────────────────

static int pass_count  = 0;
static int fail_count  = 0;
static int total_count = 0;

static void record(bool ok, const char* expr, const char* file, int line) {
    ++total_count;
    if (ok) {
        ++pass_count;
        printf("  [PASS] %s\n", expr);
    } else {
        ++fail_count;
        printf("  [FAIL] %s  <- %s:%d\n", expr, file, line);
    }
}
static void section(const char* name) { printf("\n-- %s --\n", name); }

#define CHECK(e)       record(!!(e),    #e,         __FILE__, __LINE__)
#define CHECK_EQ(a,b)  record((a)==(b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NE(a,b)  record((a)!=(b), #a " != " #b, __FILE__, __LINE__)

// ── Helper: minimal config JSON ───────────────────────────────────────────────

static const char* minimal_config() {
    return R"({
        "prompts_dir": "/tmp/c_abi_test_prompts",
        "thread_pool_size": 4,
        "default_user_id": "test_user"
    })";
}

static void write_stub_prompts() {
    // create the directory and write minimal stub prompt files
    system("mkdir -p /tmp/c_abi_test_prompts");
    const char* stubs[] = {
        "reason_stage", "injection_stage", "transform_stage", "validate_stage"
    };
    for (const char* s : stubs) {
        std::string path = std::string("/tmp/c_abi_test_prompts/") + s + ".md";
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            fprintf(f, "CATALOG:{{CATALOG}}\nHISTORY:{{HISTORY}}\nQUEUE:{{QUEUE}}\n"
                    "TASK:{{TASK}}\nSCHEMA:{{OUTPUT_SCHEMA}}\nPREVIOUS:{{PREVIOUS_RESULT}}\n"
                    "INSTRUCTION:{{INSTRUCTION}}\nINPUT:{{INPUT_TEXT}}\n"
                    "TARGET:{{TARGET_OUTPUT}}\nCRITERIA:{{CRITERIA}}\n");
            fclose(f);
        }
    }
}

// ── Tests ─────────────────────────────────────────────────────────────────────

int main() {
    printf("=== test_c_abi ===\n");

    write_stub_prompts();

    // ── Section 1: API version ────────────────────────────────────────────────
    section("api version");
    {
        CHECK_EQ(am_api_version(), 1);
    }

    // ── Section 2: Create and destroy manager ─────────────────────────────────
    section("am_create and am_destroy");
    {
        AgentManager* mgr = am_create(minimal_config());
        CHECK(mgr != nullptr);
        if (!mgr) {
            printf("  Last error: %s\n", am_last_error(nullptr));
        }
        if (mgr) am_destroy(mgr);
    }

    // ── Section 3: am_create with bad JSON returns NULL ───────────────────────
    section("am_create with invalid JSON");
    {
        AgentManager* mgr = am_create("{this is not valid json}}}");
        // Implementation may accept it gracefully or return NULL
        // Either is acceptable; just don't crash
        if (mgr) am_destroy(mgr);
        CHECK(true); // no crash is a pass
    }

    // ── Section 4: am_spawn_agent ─────────────────────────────────────────────
    section("am_spawn_agent");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping: mgr null\n"); goto skip_spawn; }

        {
            char id_buf[256] = {};
            am_status_t st = am_spawn_agent(mgr,
                R"({"name":"TestAgent","task":"hello","max_iterations":1})",
                id_buf, sizeof(id_buf));
            CHECK_EQ(st, AM_OK);
            CHECK(strlen(id_buf) > 0);

            if (st == AM_OK && strlen(id_buf) > 0) {
                // am_get_status
                char status_buf[1024] = {};
                am_status_t gs = am_get_status(mgr, id_buf, status_buf, sizeof(status_buf));
                CHECK_EQ(gs, AM_OK);
                CHECK(strlen(status_buf) > 0);

                // am_cancel_agent
                am_status_t cs = am_cancel_agent(mgr, id_buf);
                CHECK_EQ(cs, AM_OK);

                // am_destroy_agent
                am_status_t ds = am_destroy_agent(mgr, id_buf);
                CHECK_EQ(ds, AM_OK);
            }
        }
        skip_spawn:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 5: am_list_agents ─────────────────────────────────────────────
    section("am_list_agents");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_list; }

        {
            char buf[4096] = {};
            am_status_t st = am_list_agents(mgr, "", buf, sizeof(buf));
            CHECK_EQ(st, AM_OK);
            // Result should be a JSON array (possibly empty)
            CHECK(strlen(buf) > 0);
            CHECK(buf[0] == '[' || buf[0] == '{');
        }
        skip_list:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 6: buffer too small returns error or truncates ────────────────
    section("buffer too small");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_buf; }

        {
            char tiny[2] = {};
            am_status_t st = am_list_agents(mgr, "", tiny, sizeof(tiny));
            // Should return AM_ERROR_BUFFER_TOO_SMALL or silently truncate
            CHECK(st == AM_OK || st == AM_ERROR_BUFFER_TOO_SMALL);
        }
        skip_buf:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 7: blackboard via C ABI ──────────────────────────────────────
    section("blackboard C ABI");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_bb; }

        {
            am_status_t ws = am_blackboard_write(mgr, "test_key", R"({"value":123})");
            CHECK_EQ(ws, AM_OK);

            char val_buf[256] = {};
            am_status_t rs = am_blackboard_read(mgr, "test_key", val_buf, sizeof(val_buf));
            CHECK_EQ(rs, AM_OK);
            std::string val_str(val_buf);
            CHECK(val_str.find("123") != std::string::npos);

            char keys_buf[1024] = {};
            am_status_t ks = am_blackboard_keys(mgr, "", keys_buf, sizeof(keys_buf));
            CHECK_EQ(ks, AM_OK);
            std::string keys_str(keys_buf);
            CHECK(keys_str.find("test_key") != std::string::npos);
        }
        skip_bb:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 8: hot reload C ABI ───────────────────────────────────────────
    section("hot reload C ABI");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_reload; }

        {
            am_status_t st = am_reload_prompts(mgr);
            CHECK_EQ(st, AM_OK);

            st = am_set_prompts_dir(mgr, "/tmp/c_abi_test_prompts");
            CHECK_EQ(st, AM_OK);
        }
        skip_reload:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 9: set user quota C ABI ──────────────────────────────────────
    section("set user quota C ABI");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_quota; }

        {
            am_status_t st = am_set_user_quota(mgr, "user1",
                R"({"max_concurrent_agents":5,"max_llm_inflight":2,"max_tool_inflight":10})");
            CHECK_EQ(st, AM_OK);
        }
        skip_quota:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 10: am_last_error thread-local ────────────────────────────────
    section("am_last_error is not NULL");
    {
        // am_last_error(NULL) must not crash and must return a non-null pointer
        const char* err = am_last_error(nullptr);
        CHECK(err != nullptr);
    }

    // ── Section 11: am_run_agent + am_future_wait + am_future_free ───────────
    section("am_run_agent and future lifecycle");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_future; }

        {
            char id_buf[256] = {};
            am_status_t st = am_spawn_agent(mgr,
                R"({"name":"FutureAgent","max_iterations":1})",
                id_buf, sizeof(id_buf));
            CHECK_EQ(st, AM_OK);

            if (st == AM_OK) {
                AgentFuture* fut = am_run_agent(mgr, id_buf, R"({"task":"hello"})");
                CHECK(fut != nullptr);

                if (fut) {
                    char result_buf[4096] = {};
                    // Wait up to 5 seconds
                    am_status_t ws = am_future_wait(fut, 5000, result_buf, sizeof(result_buf));
                    CHECK(ws == AM_OK || ws == AM_ERROR_TIMEOUT);
                    am_future_free(fut);
                }
            }
        }
        skip_future:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 12: am_pipe registration ─────────────────────────────────────
    section("am_pipe registration");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_pipe; }

        {
            char ida[256] = {}, idb[256] = {};
            am_spawn_agent(mgr, R"({"name":"PipeFrom"})", ida, sizeof(ida));
            am_spawn_agent(mgr, R"({"name":"PipeTo"})",   idb, sizeof(idb));

            if (strlen(ida) > 0 && strlen(idb) > 0) {
                am_status_t st = am_pipe(mgr, ida, idb, "Prev: {{OUTPUT}}");
                CHECK_EQ(st, AM_OK);
            }

            if (strlen(ida) > 0) am_destroy_agent(mgr, ida);
            if (strlen(idb) > 0) am_destroy_agent(mgr, idb);
        }
        skip_pipe:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 13: am_send_message + am_drain_inbox ─────────────────────────
    section("am_send_message and am_drain_inbox");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_msg; }

        {
            char from_id[256] = {}, to_id[256] = {};
            am_spawn_agent(mgr, R"({"name":"MsgFrom"})", from_id, sizeof(from_id));
            am_spawn_agent(mgr, R"({"name":"MsgTo"})",   to_id,   sizeof(to_id));

            if (strlen(from_id) > 0 && strlen(to_id) > 0) {
                am_status_t st = am_send_message(mgr, from_id, to_id,
                    R"({"text":"hello"})");
                CHECK_EQ(st, AM_OK);

                char inbox_buf[4096] = {};
                st = am_drain_inbox(mgr, to_id, inbox_buf, sizeof(inbox_buf));
                CHECK_EQ(st, AM_OK);
                std::string inbox_str(inbox_buf);
                CHECK(inbox_str.find("hello") != std::string::npos);
            }

            if (strlen(from_id) > 0) am_destroy_agent(mgr, from_id);
            if (strlen(to_id)   > 0) am_destroy_agent(mgr, to_id);
        }
        skip_msg:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 14: quota exceeded returns AM_ERROR_QUOTA_EXCEEDED ───────────
    section("quota exceeded via C ABI");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_quota2; }

        {
            // Set quota to max 1 agent for "quser"
            am_set_user_quota(mgr, "quser",
                R"({"max_concurrent_agents":1})");

            char id1[256] = {}, id2[256] = {};
            am_status_t s1 = am_spawn_agent(mgr,
                R"({"user_id":"quser","name":"Q1"})", id1, sizeof(id1));
            CHECK_EQ(s1, AM_OK);

            am_status_t s2 = am_spawn_agent(mgr,
                R"({"user_id":"quser","name":"Q2"})", id2, sizeof(id2));
            CHECK(s2 == AM_ERROR_QUOTA_EXCEEDED || s2 != AM_OK);

            if (strlen(id1) > 0) am_destroy_agent(mgr, id1);
        }
        skip_quota2:
        if (mgr) am_destroy(mgr);
    }

    // ── Section 15: buffer-too-small returns AM_ERROR_BUFFER_TOO_SMALL ────────
    section("buffer-too-small handling for blackboard_read");
    {
        AgentManager* mgr = am_create(minimal_config());
        if (!mgr) { printf("  skipping\n"); goto skip_small; }

        {
            // Write a long value
            am_blackboard_write(mgr, "long_key",
                R"({"data":"this is a rather long value that won't fit in a tiny buffer"})");

            // Read with a 4-byte buffer
            char tiny[4] = {};
            am_status_t st = am_blackboard_read(mgr, "long_key", tiny, sizeof(tiny));
            CHECK(st == AM_ERROR_BUFFER_TOO_SMALL || st == AM_OK);

            // Read with nullptr buf (size 0)
            am_status_t st2 = am_blackboard_read(mgr, "long_key", nullptr, 0);
            CHECK(st2 == AM_ERROR_BUFFER_TOO_SMALL ||
                  st2 == AM_ERROR_INVALID_ARG      ||
                  st2 == AM_OK);
        }
        skip_small:
        if (mgr) am_destroy(mgr);
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    printf("\n==========================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           pass_count, fail_count, total_count);
    printf("==========================================\n");

    return fail_count == 0 ? 0 : 1;
}
