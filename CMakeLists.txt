# CMakeLists.txt
# ─────────────────────────────────────────────────────────────────────────────
# Build system for llama_functions — a C++ library mirroring ollama_functions.py
#
# Requirements:
#   - CMake 3.16+
#   - A C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
#   - llama.cpp source tree (or installed headers + library)
#   - nlohmann/json (fetched automatically by FetchContent if not found)
#
# Quick build:
#   mkdir build && cd build
#   cmake .. -DLLAMA_DIR=/path/to/llama.cpp
#   make -j$(nproc)
#
# Or, if llama.cpp is installed system-wide:
#   cmake ..
#   make -j$(nproc)
# ─────────────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.16)

# ── Project metadata ──────────────────────────────────────────────────────────

project(
    llama_functions
    VERSION     1.0.0
    DESCRIPTION "C++ library mirroring ollama_functions.py using llama.cpp"
    LANGUAGES   CXX
)

# ── C++ standard ──────────────────────────────────────────────────────────────

# Require C++17 — used for std::optional, structured bindings, if-constexpr
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)  # Disables GNU extensions for portability

# ── Build type default ────────────────────────────────────────────────────────

# Default to Release if not explicitly set — avoids accidental debug builds
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type not set — defaulting to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ── Find llama.cpp ────────────────────────────────────────────────────────────
#
# Strategy 1: User points CMake at a llama.cpp source tree via -DLLAMA_DIR=...
# Strategy 2: llama.cpp is already installed (find_package looks in system paths)
# Strategy 3: FetchContent downloads llama.cpp from GitHub at configure time

option(LLAMA_DIR "" "Path to the llama.cpp source directory (optional)")

if(LLAMA_DIR)
    # ── Strategy 1: add llama.cpp as a subdirectory ───────────────────────────
    message(STATUS "Using llama.cpp from: ${LLAMA_DIR}")
    # llama.cpp's own CMakeLists.txt defines the 'llama' and 'common' targets
    add_subdirectory(${LLAMA_DIR} ${CMAKE_BINARY_DIR}/llama_cpp EXCLUDE_FROM_ALL)
    set(LLAMA_TARGET llama)  # Target name defined by llama.cpp's build system
else()
    # ── Strategy 2: try to find an installed copy ─────────────────────────────
    find_package(llama QUIET)
    if(llama_FOUND)
        message(STATUS "Found installed llama.cpp package")
        set(LLAMA_TARGET llama::llama)
    else()
        # ── Strategy 3: download from GitHub using FetchContent ───────────────
        message(STATUS "llama.cpp not found — downloading from GitHub")
        include(FetchContent)
        FetchContent_Declare(
            llama_cpp
            GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
            GIT_TAG        master  # Pin to a specific commit/tag in production
            GIT_SHALLOW    TRUE    # Only fetch the latest commit (faster)
        )
        # Build only what we need — skip tests, examples, and extra tools
        set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(llama_cpp)
        set(LLAMA_TARGET llama)
    endif()
endif()

# ── Find / fetch nlohmann/json ────────────────────────────────────────────────
#
# nlohmann/json is header-only, so all we need is the include directory.
# We try find_package first, then fall back to FetchContent.

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found — downloading via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    # Build only the header-only interface; skip tests and benchmarks
    set(JSON_BuildTests    OFF CACHE BOOL "" FORCE)
    set(JSON_Install       OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── Define the library target ─────────────────────────────────────────────────

# Build as a static library by default (easier to link into other projects)
# Change STATIC → SHARED to build a .so / .dll instead
add_library(llama_functions STATIC
    llama_functions.cpp
    llama_functions.h    # Listed here so IDEs show it in the project tree
)

# ── Include directories ───────────────────────────────────────────────────────

target_include_directories(llama_functions
    PUBLIC
        # Allow consumers to #include "llama_functions.h" without a prefix
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ── Link dependencies ─────────────────────────────────────────────────────────

target_link_libraries(llama_functions
    PUBLIC
        # llama.cpp — provides llama_model*, llama_context*, etc.
        ${LLAMA_TARGET}
        # nlohmann/json — header-only; adds the include path for our users too
        nlohmann_json::nlohmann_json
)

# ── Compiler warnings ─────────────────────────────────────────────────────────

target_compile_options(llama_functions PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter   # llama.cpp headers trigger this frequently
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /wd4100   # unreferenced formal parameter (same as -Wno-unused-parameter)
    >
)

# ── Optional: example / demo executable ──────────────────────────────────────
#
# Builds a small demo that exercises generate(), embed(), similarity(),
# search(), and the full LlamaChat conversation flow.
# Disable with: cmake .. -DBUILD_LLAMA_DEMO=OFF

option(BUILD_LLAMA_DEMO "Build the llama_functions demo executable" ON)

if(BUILD_LLAMA_DEMO)
    add_executable(llama_demo llama_demo.cpp)
    target_link_libraries(llama_demo PRIVATE llama_functions)
endif()

# ── Install rules (optional) ──────────────────────────────────────────────────

install(TARGETS  llama_functions
        ARCHIVE  DESTINATION lib
        LIBRARY  DESTINATION lib
        RUNTIME  DESTINATION bin)

install(FILES llama_functions.h
        DESTINATION include)

# ── Summary ───────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "llama_functions configuration summary:")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  llama target: ${LLAMA_TARGET}")
message(STATUS "  Build demo  : ${BUILD_LLAMA_DEMO}")
message(STATUS "")
