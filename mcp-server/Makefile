# =============================================================================
# Makefile — mcp_server
#
# Builds the MCP server binary that translates LLM tool calls into graph DB
# commands over TCP.
#
# Targets:
#   all   (default) — compile and link the mcp_server binary
#   clean           — remove all .o files and the binary
#
# Usage:
#   make
#   make clean
#   make CXX=clang++
# =============================================================================

# Compiler — override with  make CXX=clang++  if needed.
CXX = g++

# -std=c++17    structured bindings and other C++17 features
# -Wall -Wextra full warning coverage
# -O2           optimise without sacrificing debuggability
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# All translation units.  Order does not matter for the compiler; the linker
# resolves symbols across all object files.
SRCS = main.cpp          \
       mcp_types.cpp     \
       json_rpc.cpp      \
       tool_registry.cpp \
       db_client.cpp     \
       mcp_handler.cpp   \
       mcp_server.cpp

# Derive object file names from source file names.
OBJS = $(SRCS:.cpp=.o)

# Final executable name.
TARGET = mcp_server

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean

all: $(TARGET)

# Link all object files into the final binary.
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Pattern rule: compile each .cpp into a .o.
# $< = source file, $@ = output object file.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Remove all build artifacts.
clean:
	rm -f $(OBJS) $(TARGET)
