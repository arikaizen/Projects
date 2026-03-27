# ==============================================================================
# Makefile — llama_demo  (no cmake required)
# ==============================================================================
#
# First time setup:
#   make setup        # clones llama.cpp locally
#   make              # builds everything with g++ only
#
# Run:
#   make run MODEL=/path/to/model.gguf
#
# Clean:
#   make clean        # removes your object files and binary
#   make clean-all    # also removes the cloned llama.cpp folder
# ==============================================================================

# ── Compiler ──────────────────────────────────────────────────────────────────
CXX    ?= g++
CC     ?= gcc
JOBS   ?= $(shell nproc 2>/dev/null || echo 4)

# ── llama.cpp location ──────────────────────────────────────────────────────
# Override with: make LLAMA_DIR=/some/other/path
LLAMA_DIR ?= ./llama.cpp

# ── nlohmann/json location (auto-detected) ───────────────────────────────
NLOHMANN_PROBE := $(LLAMA_DIR)/examples $(LLAMA_DIR) /usr/include /usr/local/include
NLOHMANN_DIR   ?= $(firstword $(foreach d,$(NLOHMANN_PROBE),$(if $(wildcard $(d)/nlohmann/json.hpp),$(d),)))

# ── Includes ──────────────────────────────────────────────────────────────────
INCLUDES := -I. \
            -I$(LLAMA_DIR)/include \
            -I$(LLAMA_DIR) \
            -I$(LLAMA_DIR)/ggml/include

ifneq ($(NLOHMANN_DIR),)
    INCLUDES += -I$(NLOHMANN_DIR)
endif

# ── Static libraries produced by llama.cpp’s own Makefile ────────────────────
LLAMA_LIB  := $(LLAMA_DIR)/libllama.a
GGML_LIB   := $(LLAMA_DIR)/libggml.a

LDFLAGS := $(LLAMA_LIB) $(GGML_LIB) -lpthread -ldl -lstdc++

# ── Your source files ──────────────────────────────────────────────────────────
SRCS   := llama_functions.cpp llama_demo.cpp
OBJS   := $(SRCS:.cpp=.o)
TARGET := llama_demo

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(INCLUDES)

# ── Model path for ‘make run’ ────────────────────────────────────────────────────
MODEL ?= model.gguf

# ==============================================================================
# Targets
# ==============================================================================

.PHONY: all setup build run clean clean-all help

## all: build llama_demo (default)
all: build

# ─────────────────────────────────────────────────────────────────────────────
## setup: clone llama.cpp into ./llama.cpp  (run this once)
setup:
	@if [ -d "$(LLAMA_DIR)" ]; then \
	    echo "[OK] $(LLAMA_DIR) already exists, skipping clone."; \
	else \
	    echo "Cloning llama.cpp into $(LLAMA_DIR)..."; \
	    git clone https://github.com/ggml-org/llama.cpp $(LLAMA_DIR) --depth=1; \
	fi
	@echo ""
	@echo "Now run:  make"

# ─────────────────────────────────────────────────────────────────────────────
# Build libllama.a + libggml.a using llama.cpp’s own Makefile (no cmake)
$(LLAMA_LIB) $(GGML_LIB):
	@if [ ! -d "$(LLAMA_DIR)" ]; then \
	    echo "ERROR: $(LLAMA_DIR) not found.  Run: make setup"; \
	    exit 1; \
	fi
	@echo "Building llama.cpp libraries (this takes a minute)..."
	$(MAKE) -C $(LLAMA_DIR) libllama.a libggml.a -j$(JOBS) \
	    CC=$(CC) CXX=$(CXX)
	@echo "Done building llama.cpp."

# ─────────────────────────────────────────────────────────────────────────────
## build: compile llama_demo
build: $(LLAMA_LIB) $(GGML_LIB) $(TARGET)

$(TARGET): $(OBJS)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "============================"
	@echo " Built: ./$(TARGET)"
	@echo " Run:   make run MODEL=/path/to/model.gguf"
	@echo "============================"

%.o: %.cpp llama_functions.h
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ─────────────────────────────────────────────────────────────────────────────
## run: build then run with a model file
run: build
	@if [ ! -f "$(MODEL)" ]; then \
	    echo "ERROR: model file not found: $(MODEL)"; \
	    echo "Usage: make run MODEL=/path/to/model.gguf"; \
	    exit 1; \
	fi
	./$(TARGET) $(MODEL)

# ─────────────────────────────────────────────────────────────────────────────
## clean: remove your compiled objects and binary
clean:
	rm -f $(OBJS) $(TARGET)

## clean-all: clean + remove the cloned llama.cpp folder
clean-all: clean
	rm -rf $(LLAMA_DIR)

# ─────────────────────────────────────────────────────────────────────────────
## help: show usage
help:
	@echo ""
	@echo "  make setup                  clone llama.cpp (run once)"
	@echo "  make                        build llama_demo"
	@echo "  make run MODEL=my.gguf      build and run with a model"
	@echo "  make clean                  remove binary and .o files"
	@echo "  make clean-all              clean + delete llama.cpp folder"
	@echo ""
	@echo "  Override LLAMA_DIR if you already have llama.cpp elsewhere:"
	@echo "  make LLAMA_DIR=~/llama.cpp"
	@echo ""
