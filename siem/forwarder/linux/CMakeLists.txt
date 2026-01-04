cmake_minimum_required(VERSION 3.10)
project(LinuxSystemLogForwarder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/inc)

# Find systemd library
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

# Source files
set(SOURCES
    src/main.cpp
    src/log_forwarder.cpp
    src/journal_reader.cpp
    src/json_utils.cpp
    src/forwarder_api.cpp
)

# Header files (for IDE organization)
set(HEADERS
    inc/log_forwarder.h
    inc/journal_reader.h
    inc/json_utils.h
    inc/forwarder_api.h
)

# Add executable
add_executable(log_forwarder ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(log_forwarder
    ${SYSTEMD_LIBRARIES}
    pthread
)

# Include systemd headers
target_include_directories(log_forwarder PRIVATE ${SYSTEMD_INCLUDE_DIRS})

# Set output directory to project bin folder
set_target_properties(log_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Test executable
add_executable(test_forwarder
    tst/test_forwarder.cpp
)

# Set test output directory to bin folder
set_target_properties(test_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Installation
install(TARGETS log_forwarder test_forwarder
    RUNTIME DESTINATION bin
)
