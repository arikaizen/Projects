# ==============================================================================
# Makefile — convo_test (CUDA GPU Version)
# ==============================================================================

CXX        := g++
CUDA_PATH  ?= /usr/local/cuda
LLAMA_DIR  ?= /home/blueberry/Projects/AI/llama.cpp
BUILD_DIR  := build

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := -I. \
            -I$(LLAMA_DIR)/include \
            -I$(LLAMA_DIR)/ggml/include \
            -I$(LLAMA_DIR)/src \
            -I$(CUDA_PATH)/include

# ── Libraries (Paths matched to your 'find' results) ──────────────────────────
# All .so files were located in llama.cpp/build/bin
LIB_BIN_PATH := $(LLAMA_DIR)/build/bin
CUDA_LIB_64  := $(CUDA_PATH)/lib64

# -L: Where the linker looks during compilation
# -Wl,-rpath: Where the OS looks when you run the program
LIBS := -L$(LIB_BIN_PATH) -Wl,-rpath,$(LIB_BIN_PATH) \
        -lllama -lggml -lggml-base -lggml-cuda -lggml-cpu \
        -L$(CUDA_LIB_64) -Wl,-rpath,$(CUDA_LIB_64) \
        -lcudart -lcublas -lcuda \
        -lpthread -ldl -lm

# ── Compiler Flags ────────────────────────────────────────────────────────────
# -DGGML_USE_CUDA tells the code to activate GPU code paths
CXXFLAGS := -O3 -std=c++17 $(INCLUDES) -DGGML_USE_CUDA -Wall -Wextra

TARGET := $(BUILD_DIR)/convo_test
SRCS   := convo.cpp convo_test1.cpp
OBJS   := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# ==============================================================================
.PHONY: all clean run help

all: $(TARGET)
	@echo ""
	@echo "===================================================="
	@echo " Built GPU-enabled: $(TARGET)"
	@echo " Running on NVIDIA RTX 5070 Ti"
	@echo "===================================================="

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: %.cpp convo.hpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

## run: build then run unit tests
run: all
	./$(TARGET)

## clean: remove build directory
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned project build."

help:
	@echo "Usage:"
	@echo "  make LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make run"
	@echo "  make clean"
