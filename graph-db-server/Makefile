# =============================================================================
# Makefile — graph_db_server
#
# Targets:
#   all   (default) — compile and link the server binary
#   clean           — remove all compiled objects and the binary
#
# Usage:
#   make              # build with default settings
#   make clean        # remove build artifacts
# =============================================================================

# C++ compiler.  Override on the command line if needed, e.g.:
#   make CXX=clang++
CXX = g++

# Compiler flags:
#   -std=c++17   Required for structured bindings (auto& [k, v]) and other
#                C++17 features used throughout the project.
#   -Wall        Enable all common warnings.
#   -Wextra      Enable additional warnings not covered by -Wall.
#   -O2          Optimize for speed without sacrificing debuggability too much.
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# All translation units that make up the server.
# Adding a new component requires adding its .cpp file here.
SRCS = main.cpp          \
       graph_types.cpp   \
       graph_store.cpp   \
       persistence.cpp   \
       protocol.cpp      \
       server.cpp        \
       request_handler.cpp

# Derive the list of object files from SRCS by substituting the .cpp suffix.
OBJS = $(SRCS:.cpp=.o)

# Name of the output executable.
TARGET = graph_db_server

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean

# Default target — build the executable.
all: $(TARGET)

# Link all object files into the final binary.
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Pattern rule: compile any .cpp file into a .o file.
# $< expands to the source file, $@ to the target object file.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Remove all build artifacts.  The '-' prefix suppresses errors if files
# do not exist (e.g. on a fresh checkout with no prior build).
clean:
	rm -f $(OBJS) $(TARGET)
