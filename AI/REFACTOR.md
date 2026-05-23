# Refactor: monolithic convo.* → abstract base + two backends

## What changed

The original `convo.hpp` / `convo.cpp` was a single-file library tightly coupled to llama.cpp. It contained `AIModelLocal` (model) and `AIConvo` (conversation) both depending on `llama.h` directly.

The new structure splits this into three layers:

| File(s) | Purpose |
|---|---|
| `types.hpp` | `Role`, `Message`, `RoleToStr`, `RoleFromStr` — no backend dependency |
| `aimodel.hpp` / `aimodel.cpp` | Abstract `AIModel` base: `Generate`, `Embed`, `Similarity`, `Search`, embedding cache |
| `aiconvo.hpp` / `aiconvo.cpp` | Abstract `AIConvo` base: `Chat`, history, save/load JSON, summary, title |
| `aimodel_llama.hpp` / `aimodel_llama.cpp` | `AIModelLlama` — llama.cpp backend (GGUF, CUDA) |
| `aiconvo_llama.hpp` / `aiconvo_llama.cpp` | `AIConvoLlama` — llama.cpp conversation with KV cache, SaveState/LoadState |
| `aimodel_vllm.hpp` / `aimodel_vllm.cpp` | `AIModelVLLM` — HTTP backend via vLLM / OpenAI-compatible API |
| `aiconvo_vllm.hpp` / `aiconvo_vllm.cpp` | `AIConvoVLLM` — conversation using vLLM chat completions endpoint |

**Bug fixed:** Old `AIModelLocal::Embed` had `if (VecNorm(result) == 0.0f){}` — the empty braces made the throw unconditional. Fixed in `aimodel.cpp` as `if (VecNorm(result) == 0.0f) { throw ...; }`.

## How to switch backends

### Local GGUF model (llama.cpp)

```cpp
#include "aimodel_llama.hpp"
#include "aiconvo_llama.hpp"

AIModelLlama model("my_model.gguf", /*ctx=*/4096, /*threads=*/4);
AIConvoLlama convo(model, "You are a helpful assistant.");
std::string reply = convo.Chat("Hello!");
```

### Remote vLLM server

```cpp
#include "aimodel_vllm.hpp"
#include "aiconvo_vllm.hpp"

AIModelVLLM model("http://localhost:8000", "meta-llama/Llama-3-8b-instruct");
AIConvoVLLM convo(model, "You are a helpful assistant.");
std::string reply = convo.Chat("Hello!");
```

Switching from local to remote is a one-line construction change — the rest of the call sites (`Chat`, `SaveHistory`, `LoadHistory`, `GetHistory`, etc.) are identical.

## How to point at a vLLM server

```cpp
AIModelVLLM model(
    "http://my-server:8000",          // base URL
    "meta-llama/Llama-3-8b-instruct", // model name (sent in every request)
    "my-secret-api-key",              // optional Bearer token
    "BAAI/bge-m3",                    // optional separate embedding model
    120,                              // HTTP timeout in seconds
    8192                              // reported context length
);
```

If `embed_model_name` is empty, the same model is used for both generation and embeddings.

The `ChatCompletion` method on `AIModelVLLM` is also exposed directly if you need raw `/v1/chat/completions` access with a custom JSON messages array.
