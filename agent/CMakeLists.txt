cmake_minimum_required(VERSION 3.17)
project(agent_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────

# nlohmann/json — try system install first, then FetchContent single-header.
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package; fetching single header...")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json_header
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/include/nlohmann
    )
    FetchContent_MakeAvailable(nlohmann_json_header)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE ${CMAKE_BINARY_DIR}/include)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
else()
    message(STATUS "nlohmann_json found: ${nlohmann_json_VERSION}")
endif()

# Threads (pthreads on Linux, native on Windows/macOS)
find_package(Threads REQUIRED)

# ── Include paths ─────────────────────────────────────────────────────────────

set(AGENT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent/stages
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent/actions
)

# ── agent_core static library ─────────────────────────────────────────────────

set(AGENT_CORE_SOURCES
    src/agent/work_item.cpp
    src/agent/work_factory.cpp
    src/agent/thread_pool.cpp
    src/agent/prompt_loader.cpp
    src/agent/event_bus.cpp
    src/agent/blackboard.cpp
    src/agent/message_inbox.cpp
    src/agent/quota.cpp
    src/agent/agent_context.cpp
    src/agent/batch_executor.cpp
    src/agent/agent.cpp
    src/agent/agent_manager.cpp
    src/agent/stages/reason_stage.cpp
    src/agent/stages/injection_stage.cpp
    src/agent/stages/transform_stage.cpp
    src/agent/stages/validate_stage.cpp
    src/agent/actions/bash_action.cpp
    src/agent/actions/read_action.cpp
    src/agent/actions/write_action.cpp
    src/agent/actions/edit_action.cpp
    src/agent/actions/glob_action.cpp
    src/agent/actions/grep_action.cpp
    src/agent/actions/web_fetch_action.cpp
    src/agent/actions/web_search_action.cpp
    src/agent/actions/task_action.cpp
    src/agent/actions/todo_write_action.cpp
    src/agent/actions/mcp_tool_action.cpp
    src/agent/actions/memory_actions.cpp
    src/agent/actions/messaging_actions.cpp
    src/agent/actions/blackboard_actions.cpp
)

add_library(agent_core STATIC ${AGENT_CORE_SOURCES})
set_target_properties(agent_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(agent_core PUBLIC ${AGENT_INCLUDE_DIRS})
target_link_libraries(agent_core PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_compile_options(agent_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# When a dedicated sanitizer build directory is configured with e.g.
#   cmake -DSANITIZER_FLAGS=-fsanitize=thread ..
# instrument agent_core itself so the sanitizer sees every memory access in the
# core (not just in the test translation unit).  Tests pick up the same flags
# via add_agent_test() below.
if(DEFINED SANITIZER_FLAGS)
    target_compile_options(agent_core PRIVATE ${SANITIZER_FLAGS})
    target_link_options(agent_core PUBLIC ${SANITIZER_FLAGS})
endif()

# ── agent_engine shared library (C ABI) ───────────────────────────────────────

add_library(agent_engine SHARED src/c_api/c_api.cpp)
target_include_directories(agent_engine PUBLIC ${AGENT_INCLUDE_DIRS})
target_link_libraries(agent_engine PRIVATE agent_core)
set_target_properties(agent_engine PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   1
    PUBLIC_HEADER include/agent_engine/c_api.h
)

# On Linux, hide all symbols except those explicitly marked as part of the
# public C API, to avoid accidental ABI pollution from internal C++ symbols.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(agent_engine PRIVATE
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/agent_engine.map
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────

enable_testing()

# Helper function: declare a test executable, link it against agent_core, and
# register it with CTest.
function(add_agent_test name)
    add_executable(${name} tests/agent/${name}.cpp)
    target_include_directories(${name} PRIVATE ${AGENT_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE agent_core)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_test(NAME ${name} COMMAND ${name})
    # Propagate sanitizer flags from the command line if present.
    if(DEFINED SANITIZER_FLAGS)
        target_compile_options(${name} PRIVATE ${SANITIZER_FLAGS})
        target_link_options(${name} PUBLIC ${SANITIZER_FLAGS})
    endif()
endfunction()

add_agent_test(test_factory)
add_agent_test(test_references)
add_agent_test(test_prompt_loader)
add_agent_test(test_agent_loop)
add_agent_test(test_chains)
add_agent_test(test_injection_reading1)
add_agent_test(test_injection_reading2)
add_agent_test(test_real_time_injection)
add_agent_test(test_batch_executor)
add_agent_test(test_dependency_dag)
add_agent_test(test_cycle_detection)
add_agent_test(test_concurrency)
add_agent_test(test_starvation)
add_agent_test(test_quotas)
add_agent_test(test_cancellation)
add_agent_test(test_determinism)
add_agent_test(test_pattern_a_delegation)
add_agent_test(test_pattern_b_messaging)
add_agent_test(test_pattern_c_blackboard)
add_agent_test(test_fan_out_in)

# The C ABI test links against the shared library (not agent_core directly)
# so it exercises the real ABI boundary.
add_executable(test_c_abi tests/agent/test_c_abi.cpp)
target_include_directories(test_c_abi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(test_c_abi PRIVATE agent_engine)
target_compile_options(test_c_abi PRIVATE -Wall -Wextra)
add_test(NAME test_c_abi COMMAND test_c_abi)

# ── Sanitizer targets ─────────────────────────────────────────────────────────

# A separate static library compiled with ThreadSanitizer.  Used by the
# check-tsan custom target and can be linked by individual test executables
# by setting SANITIZER_FLAGS=-fsanitize=thread at configure time.
add_library(agent_core_tsan STATIC ${AGENT_CORE_SOURCES})
target_include_directories(agent_core_tsan PUBLIC ${AGENT_INCLUDE_DIRS})
target_link_libraries(agent_core_tsan PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_compile_options(agent_core_tsan PRIVATE
    -fsanitize=thread
    -Wall
    -Wextra
)
target_link_options(agent_core_tsan PUBLIC -fsanitize=thread)

# Convenience target: rebuild the TSan library (used as a dependency check).
add_custom_target(check-tsan
    COMMAND ${CMAKE_COMMAND} -E echo
        "-- TSan library built successfully. Run tests with:"
    COMMAND ${CMAKE_COMMAND} -E echo
        "     cmake -DSANITIZER_FLAGS=-fsanitize=thread .. && make && ctest"
    DEPENDS agent_core_tsan
    VERBATIM
)

# ── CLI example ───────────────────────────────────────────────────────────────

add_executable(cli_driver examples/cli_driver.cpp)
target_include_directories(cli_driver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(cli_driver PRIVATE agent_engine)
target_compile_options(cli_driver PRIVATE -Wall -Wextra)

# ── Install rules ─────────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS agent_core agent_engine
    LIBRARY   DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE   DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/agent_engine
)

install(DIRECTORY include/agent
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY prompts/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/agent_engine/prompts
)

# ── Summary ───────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "agent_engine ${PROJECT_VERSION}")
message(STATUS "  C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
