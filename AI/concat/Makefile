# ==============================================================================
# Makefile — prompt concat utilities (optionally links convo + llama.cpp)
# ==============================================================================
#
# Targets:
# - build/promt_concat_api_demo : prompt list → combined string (CLI wrapper)
# - build/promt_concat_test     : combined prompt + AIConvo interactive chat
#
# Notes:
# - This Makefile does NOT modify or depend on AI/convo/Makefile.
# - `promt_concat_test` links llama.cpp + CUDA libs (same style as AI/convo/Makefile).
# - Pass model path at runtime:
#     ./build/promt_concat_test /path/to/model.gguf
#
# ==============================================================================

CXX        := g++
CUDA_PATH  ?= /usr/local/cuda
LLAMA_DIR  ?= /home/blueberry/Projects/AI/llama.cpp
BUILD_DIR  := build
BIN_DIR    := ../bin

# ── Include paths ─────────────────────────────────────────────────────────────
INCLUDES := -I. \
            -I../convo \
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

# ==============================================================================
.PHONY: all clean help

all: $(BIN_DIR)/promt_concat_api_demo $(BIN_DIR)/promt_concat_test $(BIN_DIR)/promt_concat_verify
	@echo ""
	@echo "===================================================="
	@echo " Built: $(BIN_DIR)/promt_concat_api_demo"
	@echo " Built: $(BIN_DIR)/promt_concat_test"
	@echo " Built: $(BIN_DIR)/promt_concat_verify"
	@echo "===================================================="

# ── promt_concat_api_demo (no llama link required, but harmless if present) ───
PROMT_CONCAT_API_DEMO_SRCS := promt_concat_api.cpp promt_concat.cpp
PROMT_CONCAT_API_DEMO_OBJS := $(PROMT_CONCAT_API_DEMO_SRCS:%.cpp=$(BUILD_DIR)/%.o)

$(BIN_DIR)/promt_concat_api_demo: $(PROMT_CONCAT_API_DEMO_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── promt_concat_test (links convo + prompt_concat + llama) ───────────────────
PROMT_CONCAT_TEST_SRCS := ../convo/convo.cpp promt_concat_api.cpp promt_concat_test.cpp
PROMT_CONCAT_TEST_OBJS := $(PROMT_CONCAT_TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o)

$(BIN_DIR)/promt_concat_test: $(PROMT_CONCAT_TEST_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# ── promt_concat_verify (no model, just print final prompt) ───────────────────
PROMT_CONCAT_VERIFY_SRCS := promt_concat_api.cpp promt_concat_verify.cpp
PROMT_CONCAT_VERIFY_OBJS := $(PROMT_CONCAT_VERIFY_SRCS:%.cpp=$(BUILD_DIR)/%.o)

$(BIN_DIR)/promt_concat_verify: $(PROMT_CONCAT_VERIFY_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Compile rule ──────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(BUILD_DIR)."

help:
	@echo "Usage:"
	@echo "  make -C AI/concat LLAMA_DIR=/path/to/llama.cpp"
	@echo "  make -C AI/concat clean"
	@echo ""
	@echo "Run:"
	@echo "  ./AI/bin/promt_concat_api_demo [promt_list_path]"
	@echo "  ./AI/bin/promt_concat_test /path/to/model.gguf"
	@echo "  ./AI/bin/promt_concat_verify [promt_list_path] [base_prompt]"

