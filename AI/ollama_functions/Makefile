# ==============================================================================
# Makefile — llama_demo
# Links against pre-built llama.cpp shared libraries. Build llama.cpp first:
#   cd /path/to/llama.cpp && cmake -B build && cmake --build build --parallel
# ==============================================================================
#
# Usage:
#   make LLAMA_DIR=/path/to/llama.cpp
#   make run  LLAMA_DIR=/path/to/llama.cpp  MODEL=/path/to/model.gguf
#   make clean
#
# ==============================================================================

CXX  ?= g++
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

# Path to your llama.cpp clone (must have been built with cmake -B build)
LLAMA_DIR ?= ./llama.cpp

# Build output directory
BUILD_DIR ?= build

# Model path for 'make run'
MODEL ?= model.gguf

# ── Pre-built library location ────────────────────────────────────────────────
LLAMA_LIB_DIR := $(LLAMA_DIR)/build/bin

# ── nlohmann/json (auto-detect) ───────────────────────────────────────────────
NLOHMANN_PROBE := $(LLAMA_DIR)/examples $(LLAMA_DIR) /usr/include /usr/local/include
NLOHMANN_DIR   ?= $(firstword $(foreach d,$(NLOHMANN_PROBE),\
                    $(if $(wildcard $(d)/nlohmann/json.hpp),$(d),)))

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := \
    -I. \
    -I$(LLAMA_DIR)/include \
    -I$(LLAMA_DIR)/ggml/include

ifneq ($(NLOHMANN_DIR),)
    INCLUDES += -I$(NLOHMANN_DIR)
endif

# ── Compiler / linker flags ───────────────────────────────────────────────────
CXXFLAGS := -O2 -std=c++17 $(INCLUDES)
LDFLAGS  := -L$(LLAMA_LIB_DIR) -lllama -lggml -lpthread -ldl \
            -Wl,-rpath,$(LLAMA_LIB_DIR)

# ── Object file paths ─────────────────────────────────────────────────────────
MY_BUILD := $(BUILD_DIR)/my

MY_SRCS := llama_functions.cpp llama_demo.cpp
MY_OBJS := $(MY_SRCS:%.cpp=$(MY_BUILD)/%.o)

TARGET := $(BUILD_DIR)/llama_demo

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all build run clean help

all: build

## build: compile and link
build: $(TARGET)
	@echo ""
	@echo "============================"
	@echo " Built:  $(TARGET)"
	@echo " Run:    make run MODEL=/path/to/model.gguf"
	@echo "============================"

# Link
$(TARGET): $(MY_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $(notdir $@)"
	$(CXX) -o $@ $^ $(LDFLAGS)

# Compile my .cpp files
$(MY_BUILD)/%.o: %.cpp llama_functions.h
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

## run: build and run with a model
run: build
	@if [ ! -f "$(MODEL)" ]; then \
	    echo "ERROR: model not found: $(MODEL)"; \
	    echo "Usage: make run MODEL=/path/to/model.gguf"; \
	    exit 1; \
	fi
	$(TARGET) $(MODEL)

## clean: remove the build/ directory
clean:
	@echo "Removing $(BUILD_DIR)/"
	@rm -rf $(BUILD_DIR)
	@echo "Done."

## help: show usage
help:
	@echo ""
	@echo "  make LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make run LLAMA_DIR=/path/to/llama.cpp MODEL=/path/to/model.gguf"
	@echo "  make clean"
	@echo ""
	@echo "  Optional:"
	@echo "  NLOHMANN_DIR=/path   if nlohmann/json is not auto-detected"
	@echo ""
