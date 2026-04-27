# =============================================================================
# Makefile — graph_db_server (component-per-folder layout)
#
# Directory layout (relative to this Makefile):
#   graph_types/       — graph_types.h / graph_types.cpp
#   graph_store/       — graph_store.h / graph_store.cpp
#   persistence/       — persistence.h / persistence.cpp
#   protocol/          — protocol.h    / protocol.cpp
#   server/            — server.h      / server.cpp
#   request_handler/   — request_handler.h / request_handler.cpp
#   main.cpp           — entry point
#   build/             — all .o files land here (auto-created)
#
# Targets:
#   all   (default) — compile and link the server binary
#   clean           — remove the build/ directory and the binary
#
# Usage:
#   make
#   make clean
#   make CXX=clang++   # override compiler
# =============================================================================

# ----------------------------------------------------------------------------
# Toolchain
# ----------------------------------------------------------------------------
CXX      = g++

# -std=c++17    structured bindings, std::string_view, inline variables
# -Wall -Wextra full warning coverage
# -O2           optimize without sacrificing debuggability
#
# -I<dir> flags add each component folder to the compiler's header search path.
# This lets every source file use  #include "graph_types.h"  (filename only)
# regardless of which subdirectory the including file lives in — the compiler
# searches all -I paths before giving up.
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 \
           -Igraph_types    \
           -Igraph_store    \
           -Ipersistence    \
           -Iprotocol       \
           -Iserver         \
           -Irequest_handler

# ----------------------------------------------------------------------------
# Paths
# ----------------------------------------------------------------------------

# Directory where all .o files are written.  Keeping objects out of the source
# tree means  make clean  is a single  rm -rf build/  with no risk of
# accidentally deleting source files.
BUILD_DIR = build

# Final executable name.
TARGET = graph_db_server

# ----------------------------------------------------------------------------
# Source → object mapping
#
# Each component's .cpp lives in its own subdirectory; the corresponding .o
# lives in $(BUILD_DIR)/ with a flat name (no nesting inside build/).
# The explicit rules below make the dependency on the source file unambiguous.
# ----------------------------------------------------------------------------
OBJS = \
    $(BUILD_DIR)/main.o            \
    $(BUILD_DIR)/graph_types.o     \
    $(BUILD_DIR)/graph_store.o     \
    $(BUILD_DIR)/persistence.o     \
    $(BUILD_DIR)/protocol.o        \
    $(BUILD_DIR)/server.o          \
    $(BUILD_DIR)/request_handler.o

# ----------------------------------------------------------------------------
# Targets
# ----------------------------------------------------------------------------

.PHONY: all clean

# Default target: ensure build/ exists, then link.
all: $(BUILD_DIR) $(TARGET)

# Create the build directory if it does not exist yet.
# The order-only prerequisite (|) means make checks for existence but does
# not rebuild targets just because the directory timestamp changed.
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link all object files into the final executable.
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ----------------------------------------------------------------------------
# Compilation rules — one explicit rule per component so that make can track
# the correct source-file dependency for incremental rebuilds.
# ----------------------------------------------------------------------------

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/graph_types.o: graph_types/graph_types.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/graph_store.o: graph_store/graph_store.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/persistence.o: persistence/persistence.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/protocol.o: protocol/protocol.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/server.o: server/server.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/request_handler.o: request_handler/request_handler.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ----------------------------------------------------------------------------
# Clean — remove all build artifacts.
# ----------------------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
