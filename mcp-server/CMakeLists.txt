cmake_minimum_required(VERSION 3.17)
project(mcp_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────

find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json_header
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/include/nlohmann)
    FetchContent_MakeAvailable(nlohmann_json_header)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE ${CMAKE_BINARY_DIR}/include)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

find_package(OpenSSL REQUIRED)

find_path(HTTPLIB_INCLUDE_DIR httplib.h)
if(NOT HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR "httplib.h not found. Set -DHTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib")
endif()

find_package(Threads REQUIRED)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(mcp_server
    src/main.cpp
    src/auth.cpp
    src/tool_registry.cpp
    src/tool_runner.cpp
)

target_include_directories(mcp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(mcp_server PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_definitions(mcp_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_options(mcp_server PRIVATE -Wall -Wextra)

install(TARGETS mcp_server RUNTIME DESTINATION bin)

# ── Install tools ─────────────────────────────────────────────────────────────
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools/
    DESTINATION share/mcp_server/tools
    FILES_MATCHING PATTERN "*.py" PATTERN "*.json")
