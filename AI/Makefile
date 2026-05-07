# ==============================================================================
# Makefile — AI tools
# ==============================================================================
#
# Targets:
# - bin/convo_manager   : multi-model convo manager CLI
# - layer_inference     : sliding-window VRAM-bounded inference engine
#                         (delegates to layer_inference/Makefile)
#
# Usage:
#   make -C AI all
#   ./AI/bin/convo_manager model1.gguf model2.gguf
#   make -C AI layer_inference
#   ./AI/layer_inference/bin/layer_inference model.gguf
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
	concat/promt_concat_api.cpp
CONVO_MANAGER_OBJS := $(CONVO_MANAGER_SRCS:%.cpp=$(BUILD_DIR)/%.o)

# ==============================================================================
.PHONY: all clean help layer_inference

all: $(CONVO_MANAGER_TARGET)

# Delegate to the component's own Makefile, forwarding LLAMA_DIR if set.
layer_inference:
	$(MAKE) -C layer_inference LLAMA_DIR=$(LLAMA_DIR) CUDA_PATH=$(CUDA_PATH)
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
	$(MAKE) -C layer_inference clean 2>/dev/null || true
	@echo "Cleaned."

help:
	@echo "Usage:"
	@echo "  make -C AI all LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make -C AI layer_inference LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make -C AI clean"
	@echo ""
	@echo "Run:"
	@echo "  ./AI/bin/convo_manager model1.gguf [model2.gguf ...]"
	@echo "  ./AI/layer_inference/bin/layer_inference model.gguf"

