# ==============================================================================
# Makefile — llama_demo & llama_functions
# ==============================================================================
#
# Usage:
#   make                        # build llama_demo (default)
#   make run MODEL=path/to.gguf # build + run with a model
#   make clean                  # remove build artefacts
#   make help                   # show this help
#
# Required variables (set on the command line or export them):
#   LLAMA_DIR   — root of your llama.cpp clone/install
#                 e.g. make LLAMA_DIR=~/llama.cpp
#
# Optional variables:
#   NLOHMANN_DIR — directory that contains the nlohmann/ subfolder
#                  (defaults to $(LLAMA_DIR)/examples  then /usr/include)
#   MODEL        — .gguf model path used by 'make run'
#                  (default: model.gguf in the current directory)
#   CXX          — C++ compiler  (default: g++)
#   CXXSTD       — C++ standard  (default: c++17)
#   OPT          — optimisation flag (default: -O2)
#   JOBS         — parallel jobs for sub-makes (default: nproc)
# ==============================================================================

# ── Compiler & flags ──────────────────────────────────────────────────────────
CXX      ?= g++
CXXSTD   ?= c++17
OPT      ?= -O2
JOBS     ?= $(shell nproc 2>/dev/null || echo 4)

# ── Paths ────────────────────────────────────────────────────────────────────
# LLAMA_DIR must be provided by the user
LLAMA_DIR ?=

# Probe common locations for nlohmann/json.hpp
ifneq ($(LLAMA_DIR),)
    NLOHMANN_PROBE := $(LLAMA_DIR)/examples $(LLAMA_DIR)/include $(LLAMA_DIR)
else
    NLOHMANN_PROBE :=
endif
NLOMANN_PROBE += /usr/include /usr/local/include

# Use NLOHMANN_DIR if explicitly set; otherwise auto-detect
ifndef NLOHMANN_DIR
    NLOHMANN_DIR := $(firstword $(foreach d,$(NLOHMANN_PROBE),$(if $(wildcard $(d)/nlohmann/json.hpp),$(d),)))
endif

# ── Derived include / library paths ──────────────────────────────────────────
INCLUDES  := -I.
ifneq ($(LLAMA_DIR),)
    INCLUDES += -I$(LLAMA_DIR)/include -I$(LLAMA_DIR)
endif
ifneq ($(NLOHMANN_DIR),)
    INCLUDES += -I$(NLOHMANN_DIR)
endif

# Library search paths — check both build/src and build (CMake layouts differ)
LIB_DIRS  :=
ifneq ($(LLAMA_DIR),)
    LIB_DIRS += -L$(LLAMA_DIR)/build/src
    LIB_DIRS += -L$(LLAMA_DIR)/build
    LIB_DIRS += -L$(LLAMA_DIR)/build/bin
endif

LDFLAGS   := $(LIB_DIRS) -lllama -lpthread

# Runtime library path so the binary finds libllama.so without LD_LIBRARY_PATH
ifneq ($(LLAMA_DIR),)
    LDFLAGS += -Wl,-rpath,$(LLAMA_DIR)/build/src
    LDFLAGS += -Wl,-rpath,$(LLAMA_DIR)/build
endif

# ── Sources & targets ────────────────────────────────────────────────────────
SRCS    := llama_functions.cpp llama_demo.cpp
OBJS    := $(SRCS:.cpp=.o)
TARGET  := llama_demo

CXXFLAGS := -std=$(CXXSTD) $(OPT) -Wall -Wextra $(INCLUDES)

# ── Default model path for 'make run' ────────────────────────────────────────
MODEL   ?= model.gguf

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all build run clean help check-deps

## all: build the demo binary (default target)
all: check-deps build

## build: compile and link llama_demo
build: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "Build successful:  ./$(TARGET)"
	@echo "Run with:          make run MODEL=/path/to/model.gguf"

%.o: %.cpp llama_functions.h
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

## run: build then run the demo with MODEL=<path>
run: build
	@if [ ! -f "$(MODEL)" ]; then \
	    echo "ERROR: model file not found: $(MODEL)"; \
	    echo "Usage: make run MODEL=/path/to/model.gguf"; \
	    exit 1; \
	fi
	./$(TARGET) $(MODEL)

## clean: remove compiled objects and the binary
clean:
	@echo "Cleaning..."
	@rm -f $(OBJS) $(TARGET)
	@echo "Done."

## check-deps: verify LLAMA_DIR and required headers/libs are present
check-deps:
	@# ── Check LLAMA_DIR ────────────────────────────────────────────────────
	@if [ -z "$(LLAMA_DIR)" ]; then \
	    echo "ERROR: LLAMA_DIR is not set."; \
	    echo "       Set it to your llama.cpp directory:"; \
	    echo "       make LLAMA_DIR=~/llama.cpp"; \
	    exit 1; \
	fi
	@# ── Check llama.h ──────────────────────────────────────────────────────
	@if [ ! -f "$(LLAMA_DIR)/include/llama.h" ] && [ ! -f "$(LLAMA_DIR)/llama.h" ]; then \
	    echo "ERROR: llama.h not found under $(LLAMA_DIR)/include/ or $(LLAMA_DIR)/"; \
	    echo "       Make sure LLAMA_DIR points to your llama.cpp root."; \
	    exit 1; \
	fi
	@# ── Check libllama ─────────────────────────────────────────────────────
	@if ! ls $(LLAMA_DIR)/build/src/libllama* $(LLAMA_DIR)/build/libllama* 2>/dev/null | grep -q .; then \
	    echo "ERROR: libllama not found. Build llama.cpp first:"; \
	    echo "       cd $(LLAMA_DIR) && cmake -B build && cmake --build build -j$(JOBS)"; \
	    exit 1; \
	fi
	@# ── Check nlohmann/json.hpp ────────────────────────────────────────────
	@if [ -z "$(NLOHMANN_DIR)" ]; then \
	    echo "ERROR: nlohmann/json.hpp not found."; \
	    echo "       Install it (sudo apt install nlohmann-json3-dev) or set:"; \
	    echo "       make NLOHMANN_DIR=/path/containing/nlohmann/"; \
	    exit 1; \
	fi
	@echo "[OK]  All dependencies found."
	@echo "      LLAMA_DIR    = $(LLAMA_DIR)"
	@echo "      NLOHMANN_DIR = $(NLOHMANN_DIR)"
	@echo ""

## help: print available targets and variables
help:
	@echo ""
	@echo "======================================"
	@echo " llama_demo Makefile"
	@echo "======================================"
	@echo ""
	@echo "Targets:"
	@grep -E '^##' $(MAKEFILE_LIST) | sed 's/^## /  /'
	@echo ""
	@echo "Variables (override on the command line):"
	@echo "  LLAMA_DIR    path to llama.cpp root           (REQUIRED)"
	@echo "  NLOHMANN_DIR path containing nlohmann/        (auto-detected)"
	@echo "  MODEL        .gguf model file for 'make run'  (default: model.gguf)"
	@echo "  CXX          C++ compiler                     (default: g++)"
	@echo "  OPT          optimisation level               (default: -O2)"
	@echo ""
	@echo "Examples:"
	@echo "  make LLAMA_DIR=~/llama.cpp"
	@echo "  make run LLAMA_DIR=~/llama.cpp MODEL=~/models/mistral.gguf"
	@echo "  make clean"
	@echo ""
