cmake_minimum_required(VERSION 3.10)
project(WindowsEventLogForwarder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/inc)

# Source files
set(SOURCES
    src/main.cpp
    src/event_log_reader.cpp
    src/log_forwarder.cpp
)

# Main executable
add_executable(log_forwarder ${SOURCES})

# Link Windows libraries
target_link_libraries(log_forwarder
    ws2_32      # Winsock2
    wevtapi     # Windows Event Log API
)

# Set output directory
set_target_properties(log_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

# Test executable
add_executable(test_forwarder
    tst/test_forwarder.cpp
    src/event_log_reader.cpp
    src/log_forwarder.cpp
)

target_link_libraries(test_forwarder
    ws2_32
    wevtapi
)

set_target_properties(test_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

# Installation
install(TARGETS log_forwarder
    RUNTIME DESTINATION bin
)
