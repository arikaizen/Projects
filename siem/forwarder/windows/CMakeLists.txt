cmake_minimum_required(VERSION 3.10)
project(WindowsEventLogForwarder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/inc)

# Source files
set(SOURCES
    src/main.cpp
    src/log_forwarder.cpp
    src/event_log_reader.cpp
    src/json_utils.cpp
    src/forwarder_api.cpp
)

# Header files (for IDE organization)
set(HEADERS
    inc/log_forwarder.h
    inc/event_log_reader.h
    inc/json_utils.h
    inc/forwarder_api.h
)

# Add executable
add_executable(log_forwarder ${SOURCES} ${HEADERS})

# Link Windows libraries
target_link_libraries(log_forwarder
    wevtapi
    ws2_32
)

# Set output directory to project bin folder
set_target_properties(log_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Test executable
add_executable(test_forwarder
    tst/test_forwarder.cpp
)

# Link Windows libraries for test
target_link_libraries(test_forwarder
    ws2_32
)

# Set test output directory to bin folder
set_target_properties(test_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# Installation
install(TARGETS log_forwarder test_forwarder
    RUNTIME DESTINATION bin
)
