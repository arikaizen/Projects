# =============================================================================
# Makefile — mcp_server  (component-per-folder layout)
#
# Directory layout
# ----------------
#   mcp_types/      McpRequest, McpResponse, ToolCall structs
#   json_rpc/       JSON-RPC 2.0 parser and formatter
#   tool_registry/  Tool schema definitions and validation
#   db_client/      TCP client + tool-to-command translation
#   mcp_handler/    Method dispatcher (initialize, tools/list, tools/call …)
#   server/         stdin/stdout event loop (McpServer)
#   main.cpp        Entry point — wires all components together
#   build/          Object files (created automatically)
#
# Usage
# -----
#   make              build the mcp_server binary
#   make clean        remove build/ and the binary
#   make CXX=clang++  build with clang instead of g++
# =============================================================================

# Compiler — override with  make CXX=clang++  if needed.
CXX = g++

# -std=c++17    structured bindings and other C++17 features used throughout
# -Wall -Wextra full warning coverage
# -O2           optimise without sacrificing debuggability
# -I<dir>       one include path per component folder so #include "foo.h" resolves
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 \
           -Imcp_types     \
           -Ijson_rpc      \
           -Itool_registry \
           -Idb_client     \
           -Imcp_handler   \
           -Iserver

# Object files land in build/ to keep source trees clean.
BUILD_DIR = build

# Final executable name.
TARGET = mcp_server

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean

all: $(TARGET)

# Link all object files into the final binary.
$(TARGET): $(BUILD_DIR)/main.o           \
           $(BUILD_DIR)/mcp_types.o      \
           $(BUILD_DIR)/json_rpc.o       \
           $(BUILD_DIR)/tool_registry.o  \
           $(BUILD_DIR)/db_client.o      \
           $(BUILD_DIR)/mcp_handler.o    \
           $(BUILD_DIR)/mcp_server.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# -----------------------------------------------------------------------------
# Per-component compilation rules
#
# The order-only prerequisite  | $(BUILD_DIR)  creates the build directory on
# first run without marking the object file as stale when the directory's
# timestamp changes (which would force unnecessary recompiles).
# -----------------------------------------------------------------------------

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/mcp_types.o: mcp_types/mcp_types.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/json_rpc.o: json_rpc/json_rpc.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/tool_registry.o: tool_registry/tool_registry.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/db_client.o: db_client/db_client.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/mcp_handler.o: mcp_handler/mcp_handler.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/mcp_server.o: server/mcp_server.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Create the build directory (order-only — not a real build target).
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Remove all build artifacts.
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
