# ==============================================================================
# Makefile — LayerInferenceEngine
# ==============================================================================
#
# Targets:
#   all                  : build bin/layer_inference
#   clean                : remove build/ directory
#
# Override the llama.cpp location if needed:
#   make LLAMA_DIR=/path/to/llama.cpp
#
# ==============================================================================

CXX        := g++
CUDA_PATH  ?= /usr/local/cuda
LLAMA_DIR  ?= /home/blueberry/Projects/AI/llama.cpp
BUILD_DIR  := build
BIN_DIR    := bin

TARGET := $(BIN_DIR)/layer_inference

# ── Sources ────────────────────────────────────────────────────────────────────
SRCS := src/layer_inference_engine.cpp \
        src/main.cpp

OBJS := $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)

# ── Include paths ──────────────────────────────────────────────────────────────
INCLUDES := \
    -Iinclude \
    -I$(LLAMA_DIR)/include \
    -I$(LLAMA_DIR)/ggml/include \
    -I$(LLAMA_DIR)/src \
    -I$(CUDA_PATH)/include

# ── Libraries ─────────────────────────────────────────────────────────────────
LIB_BIN_PATH := $(LLAMA_DIR)/build/bin
CUDA_LIB_64  := $(CUDA_PATH)/lib64

LIBS := \
    -L$(LIB_BIN_PATH) -Wl,-rpath,$(LIB_BIN_PATH) \
    -lllama -lggml -lggml-base -lggml-cuda -lggml-cpu \
    -L$(CUDA_LIB_64) -Wl,-rpath,$(CUDA_LIB_64) \
    -lcudart -lcublas -lcuda \
    -lpthread -ldl -lm

# ── Flags ──────────────────────────────────────────────────────────────────────
CXXFLAGS := -O3 -std=c++17 $(INCLUDES) -DGGML_USE_CUDA -Wall -Wextra

# ==============================================================================
.PHONY: all clean help

all: $(TARGET)
	@echo ""
	@echo "===================================================="
	@echo " Built: $(TARGET)"
	@echo "===================================================="
	@echo " Run:   ./$(TARGET) path/to/model.gguf"
	@echo "===================================================="

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Depend on the public header so changes trigger a rebuild.
$(BUILD_DIR)/layer_inference_engine.o: include/layer_inference_engine.h
$(BUILD_DIR)/main.o: include/layer_inference_engine.h

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned."

help:
	@echo "Usage:"
	@echo "  make [LLAMA_DIR=/path/to/llama.cpp]"
	@echo "  ./$(TARGET) model.gguf"
