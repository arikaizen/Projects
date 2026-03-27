# ==============================================================================
# Makefile — llama_demo
# Compiles llama.cpp source files directly with g++. No cmake, no llama Makefile.
# ==============================================================================
#
# Usage:
#   make LLAMA_DIR=/path/to/llama.cpp
#   make run  LLAMA_DIR=/path/to/llama.cpp  MODEL=/path/to/model.gguf
#   make clean
#
# ==============================================================================

CXX  ?= g++
CC   ?= gcc
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

# Path to your llama.cpp clone
LLAMA_DIR ?= ./llama.cpp

# Build output directory
BUILD_DIR ?= build

# Model path for 'make run'
MODEL ?= model.gguf

# ── nlohmann/json (auto-detect) ────────────────────────────────────────────────────
NLOHMANN_PROBE := $(LLAMA_DIR)/examples $(LLAMA_DIR) /usr/include /usr/local/include
NLOHMANN_DIR   ?= $(firstword $(foreach d,$(NLOHMANN_PROBE),\
                    $(if $(wildcard $(d)/nlohmann/json.hpp),$(d),)))

# ── Include paths ─────────────────────────────────────────────────────────────────
INCLUDES := \
    -I. \
    -I$(LLAMA_DIR)/include \
    -I$(LLAMA_DIR)/ggml/include \
    -I$(LLAMA_DIR)/src \
    -I$(LLAMA_DIR)/ggml/src \
    -I$(LLAMA_DIR)/ggml/src/ggml-cpu

ifneq ($(NLOHMANN_DIR),)
    INCLUDES += -I$(NLOHMANN_DIR)
endif

# ── Compiler flags ────────────────────────────────────────────────────────────────
CFLAGS   := -O2 -std=c11   $(INCLUDES)
CXXFLAGS := -O2 -std=c++17 $(INCLUDES)

# ── Discover llama.cpp source files (CPU only, skip all GPU backends) ─────────────
EXCLUDE := cuda|metal|vulkan|opencl|sycl|hip|cann|rpc|kompute

# All .c files under ggml/src/ (CPU tensor library)
LLAMA_C_SRCS := $(shell find $(LLAMA_DIR)/ggml/src -name "*.c" \
                  | grep -v -E "$(EXCLUDE)")

# All .cpp files under ggml/src/ and src/ (llama model + ggml backends)
LLAMA_CPP_SRCS := $(shell find $(LLAMA_DIR)/ggml/src $(LLAMA_DIR)/src \
                    -name "*.cpp" \
                    | grep -v -E "$(EXCLUDE)")

# ── Object file paths ─────────────────────────────────────────────────────────────────
# My files   → build/my/
# llama files → build/lib/  (preserving sub-path to avoid name collisions)
MY_BUILD  := $(BUILD_DIR)/my
LIB_BUILD := $(BUILD_DIR)/lib

MY_SRCS  := llama_functions.cpp llama_demo.cpp
MY_OBJS  := $(MY_SRCS:%.cpp=$(MY_BUILD)/%.o)

LLAMA_C_OBJS   := $(patsubst $(LLAMA_DIR)/%.c,   $(LIB_BUILD)/%.o, $(LLAMA_C_SRCS))
LLAMA_CPP_OBJS := $(patsubst $(LLAMA_DIR)/%.cpp, $(LIB_BUILD)/%.o, $(LLAMA_CPP_SRCS))
LLAMA_OBJS     := $(LLAMA_C_OBJS) $(LLAMA_CPP_OBJS)

TARGET := $(BUILD_DIR)/llama_demo

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all build run clean help

all: build

## build: compile everything
build: $(TARGET)
	@echo ""
	@echo "============================"
	@echo " Built:  $(TARGET)"
	@echo " Run:    make run MODEL=/path/to/model.gguf"
	@echo "============================"

# Link
$(TARGET): $(MY_OBJS) $(LLAMA_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $(notdir $@)"
	$(CXX) -o $@ $^ -lpthread -ldl

# Compile my .cpp files
$(MY_BUILD)/%.o: %.cpp llama_functions.h
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile llama.cpp .c files
$(LIB_BUILD)/%.o: $(LLAMA_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Compile llama.cpp .cpp files
$(LIB_BUILD)/%.o: $(LLAMA_DIR)/%.cpp
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
