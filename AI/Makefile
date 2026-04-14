# ==============================================================================
# Makefile — AI tools
# ==============================================================================
#
# Targets:
# - bin/convo_manager : multi-model convo manager CLI
#
# Usage:
#   make -C AI all
#   ./AI/bin/convo_manager model1.gguf model2.gguf
#
# ==============================================================================

CXX        := g++
CUDA_PATH  ?= /usr/local/cuda
LLAMA_DIR  ?= /home/blueberry/Projects/AI/llama.cpp
BUILD_DIR  := build
BIN_DIR    := bin

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := -Iconvo \
            -Iconvo/convoManager \
            -Itools/Cmdline \
            -Iconcat \
            -Ievents \
            -Ischeduler \
            -I$(LLAMA_DIR)/include \
            -I$(LLAMA_DIR)/ggml/include \
            -I$(LLAMA_DIR)/src \
            -I$(CUDA_PATH)/include

# ── Libraries / rpath ─────────────────────────────────────────────────────────
LIB_BIN_PATH := $(LLAMA_DIR)/build/bin
CUDA_LIB_64  := $(CUDA_PATH)/lib64

LIBS := -L$(LIB_BIN_PATH) -Wl,-rpath,$(LIB_BIN_PATH) \
        -lllama -lggml -lggml-base -lggml-cuda -lggml-cpu \
        -L$(CUDA_LIB_64) -Wl,-rpath,$(CUDA_LIB_64) \
        -lcudart -lcublas -lcuda \
        -lpthread -ldl -lm

# ── Compiler flags ────────────────────────────────────────────────────────────
CXXFLAGS := -O3 -std=c++17 $(INCLUDES) -DGGML_USE_CUDA -Wall -Wextra

CONVO_MANAGER_TARGET := $(BIN_DIR)/convo_manager
CONVO_MANAGER_SRCS := \
	convo_manager_main.cpp \
	tools/Cmdline/Cmdline.cpp \
	convo/convo.cpp \
	convo/convoManager/ConvoManager.cpp \
	concat/promt_concat_api.cpp \
	scheduler/Scheduler.cpp \
	events/SaveHistoryEvent.cpp \
	events/SaveTokensEvent.cpp \
	events/SaveKvCacheEvent.cpp \
	events/SaveMetaEvent.cpp
CONVO_MANAGER_OBJS := $(CONVO_MANAGER_SRCS:%.cpp=$(BUILD_DIR)/%.o)

# ==============================================================================
.PHONY: all clean help

all: $(CONVO_MANAGER_TARGET)
	@echo ""
	@echo "===================================================="
	@echo " Built: $(CONVO_MANAGER_TARGET)"
	@echo "===================================================="

$(CONVO_MANAGER_TARGET): $(CONVO_MANAGER_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(BUILD_DIR)."

help:
	@echo "Usage:"
	@echo "  make -C AI all LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make -C AI clean"
	@echo ""
	@echo "Run:"
	@echo "  ./AI/bin/convo_manager model1.gguf [model2.gguf ...]"

