#include "bash_action.hpp"
#include "agent/agent_context.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

namespace agent {

WorkResult BashAction::execute(AgentContext& ctx) {
    using namespace std::chrono;
    auto _start = steady_clock::now();

    WorkResult result;
    result.item_id   = id;
    result.item_name = name;
    result.item_kind = "Action";

    try {
        auto resolved   = ctx.resolveReferences(inputs);
        auto command    = resolved.at("command").get<std::string>();
        int  timeout_ms = resolved.value("timeout_ms", 10000);

        std::cerr << "[ACTION:" << name << "] running command (timeout_ms=" << timeout_ms << "): "
                  << command << "\n";

        // Run command in a detached thread so we can enforce a timeout.
        std::string captured;
        int         exit_code = -1;

        std::promise<void> done_promise;
        auto               done_future = done_promise.get_future();

        auto worker = std::thread([&]() {
            FILE* pipe = popen(command.c_str(), "r");
            if (!pipe) {
                done_promise.set_value();
                return;
            }
            std::array<char, 4096> buf{};
            while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
                captured += buf.data();
            }
            int raw = pclose(pipe);
            // WEXITSTATUS on POSIX; raw on Windows (pclose returns the exit status directly).
#ifdef WEXITSTATUS
            exit_code = WEXITSTATUS(raw);
#else
            exit_code = raw;
#endif
            done_promise.set_value();
        });

        auto status = done_future.wait_for(milliseconds(timeout_ms));
        if (status == std::future_status::timeout) {
            worker.detach();
            result.success = false;
            result.error   = "Command timed out after " + std::to_string(timeout_ms) + " ms";
            result.output  = {{"stdout", ""}, {"exit_code", -1}};
        } else {
            if (worker.joinable()) worker.join();
            result.output  = {{"stdout", captured}, {"exit_code", exit_code}};
            result.success = (exit_code == 0);
            if (!result.success) {
                result.error = "Command exited with code " + std::to_string(exit_code);
            }
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error   = e.what();
    }

    result.duration  = duration_cast<milliseconds>(steady_clock::now() - _start);
    result.timestamp = system_clock::now();
    return result;
}

void registerBashAction(WorkFactory& factory) {
    WorkItemSpec spec{
        "BashAction",
        "Run a shell command and capture its stdout and exit code.",
        WorkItem::Kind::Action,
        {
            {"type", "object"},
            {"required", {"command"}},
            {"properties", {
                {"command",    {{"type", "string"}, {"description", "Shell command to execute."}}},
                {"timeout_ms", {{"type", "integer"}, {"description", "Max execution time in ms (default 10000)."}}},
            }}
        }
    };

    factory.registerItem(std::move(spec), [](std::string id, nlohmann::json inputs) {
        return std::make_unique<BashAction>(std::move(id), "BashAction", std::move(inputs));
    });
}

} // namespace agent
