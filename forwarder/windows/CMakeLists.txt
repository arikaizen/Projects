cmake_minimum_required(VERSION 3.10)
project(WindowsEventLogForwarder)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(log_forwarder
    src/main.cpp
)

# Link Windows libraries
target_link_libraries(log_forwarder
    wevtapi
    ws2_32
)

# Set output directory
set_target_properties(log_forwarder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS log_forwarder
    RUNTIME DESTINATION bin
)
