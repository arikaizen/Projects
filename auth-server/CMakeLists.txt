cmake_minimum_required(VERSION 3.17)
project(auth_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ──────────────────────────────────────────────────────────────

# nlohmann/json
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

# OpenSSL — required for RSA key generation, JWT RS256, PKCE SHA-256
find_package(OpenSSL REQUIRED)

# cpp-httplib — header-only HTTP server/client
find_path(HTTPLIB_INCLUDE_DIR httplib.h)
if(NOT HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR "httplib.h not found. Install cpp-httplib and set "
                        "-DHTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib")
endif()

find_package(Threads REQUIRED)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(auth_server
    src/main.cpp
    src/crypto.cpp
    src/store.cpp
)

target_include_directories(auth_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HTTPLIB_INCLUDE_DIR}
)

target_link_libraries(auth_server PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_definitions(auth_server PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_compile_options(auth_server PRIVATE -Wall -Wextra)

install(TARGETS auth_server RUNTIME DESTINATION bin)
