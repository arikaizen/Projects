# ==============================================================================
# Makefile — convo_test
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

# Path to your llama.cpp clone
LLAMA_DIR ?= ./llama.cpp

# Build output directory
BUILD_DIR ?= build

# Model path for 'make run' (Part 2 integration tests)
MODEL ?= model.gguf

# ── nlohmann/json (auto-detect) ───────────────────────────────────────────────
NLOHMANN_PROBE := $(LLAMA_DIR)/examples $(LLAMA_DIR) /usr/include /usr/local/include
NLOHMANN_DIR   ?= $(firstword $(foreach d,$(NLOHMANN_PROBE),\
                    $(if $(wildcard $(d)/nlohmann/json.hpp),$(d),)))

# ── Include paths ─────────────────────────────────────────────────────────────
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

# ── Compiler flags ────────────────────────────────────────────────────────────
CFLAGS   := -O2 -std=c11   $(INCLUDES)
CXXFLAGS := -O2 -std=c++17 $(INCLUDES) -Wall -Wextra

# ── llama.cpp sources (CPU only, skip GPU backends) ───────────────────────────
EXCLUDE := cuda|metal|vulkan|opencl|sycl|hip|cann|rpc|kompute

LLAMA_C_SRCS   := $(shell find $(LLAMA_DIR)/ggml/src -name "*.c"   | grep -v -E "$(EXCLUDE)")
LLAMA_CPP_SRCS := $(shell find $(LLAMA_DIR)/ggml/src $(LLAMA_DIR)/src -name "*.cpp" | grep -v -E "$(EXCLUDE)")

# ── Object paths ──────────────────────────────────────────────────────────────
MY_BUILD  := $(BUILD_DIR)/my
LIB_BUILD := $(BUILD_DIR)/lib

MY_SRCS := convo.cpp convo_test.cpp
MY_OBJS := $(MY_SRCS:%.cpp=$(MY_BUILD)/%.o)

LLAMA_C_OBJS   := $(patsubst $(LLAMA_DIR)/%.c,   $(LIB_BUILD)/%.o, $(LLAMA_C_SRCS))
LLAMA_CPP_OBJS := $(patsubst $(LLAMA_DIR)/%.cpp, $(LIB_BUILD)/%.o, $(LLAMA_CPP_SRCS))
LLAMA_OBJS     := $(LLAMA_C_OBJS) $(LLAMA_CPP_OBJS)

TARGET := $(BUILD_DIR)/convo_test

# ==============================================================================
.PHONY: all build run clean help

all: build

## build: compile everything
build: $(TARGET)
	@echo ""
	@echo "============================"
	@echo " Built: $(TARGET)"
	@echo " Unit tests (no model):  ./$(TARGET)"
	@echo " Integration tests:      ./$(TARGET) /path/to/model.gguf"
	@echo "============================"

$(TARGET): $(MY_OBJS) $(LLAMA_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $(notdir $@)"
	$(CXX) -o $@ $^ -lpthread -ldl

$(MY_BUILD)/%.o: %.cpp convo.hpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB_BUILD)/%.o: $(LLAMA_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_BUILD)/%.o: $(LLAMA_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

## run: build then run unit tests only (no model needed)
run: build
	./$(TARGET)

## run-integration: build then run all tests with a model
run-integration: build
	@if [ ! -f "$(MODEL)" ]; then \
	    echo "ERROR: model not found: $(MODEL)"; \
	    echo "Usage: make run-integration MODEL=/path/to/model.gguf"; \
	    exit 1; \
	fi
	./$(TARGET) $(MODEL)

## clean: remove build/ directory
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned."

## help: show usage
help:
	@echo ""
	@echo "  make LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make run"
	@echo "  make run-integration MODEL=/path/to/model.gguf LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make clean"
	@echo ""
	@echo "  Optional:"
	@echo "  NLOHMANN_DIR=/path   if nlohmann/json is not auto-detected"
	@echo ""
