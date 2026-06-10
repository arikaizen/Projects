# LLM Providers

The agent engine can run agents on any of the following model backends. The
same configuration works for the **default engine LLM**, **per-agent overrides**
(so mixed-provider agent groups can talk to each other), and the GUI's direct
chat path.

## Supported providers

| Provider id   | Service                    | Protocol                | Auth | Default base URL |
|---------------|----------------------------|-------------------------|------|------------------|
| `anthropic`   | Claude                     | Anthropic Messages      | API key (`x-api-key`) | `https://api.anthropic.com` |
| `openai`      | ChatGPT                    | OpenAI Chat Completions | API key (Bearer) | `https://api.openai.com` |
| `google`      | Gemini                     | Generative Language     | API key **or** Google OAuth | `https://generativelanguage.googleapis.com` |
| `groq`        | Groq                       | OpenAI-compatible       | API key | `https://api.groq.com/openai` |
| `mistral`     | Mistral                    | OpenAI-compatible       | API key | `https://api.mistral.ai` |
| `deepseek`    | DeepSeek                   | OpenAI-compatible       | API key | `https://api.deepseek.com` |
| `xai`         | Grok                       | OpenAI-compatible       | API key | `https://api.x.ai` |
| `openrouter`  | OpenRouter (many models)   | OpenAI-compatible       | API key | `https://openrouter.ai/api` |
| `together`    | Together AI                | OpenAI-compatible       | API key | `https://api.together.xyz` |
| `ollama`      | Ollama (local)             | Ollama native           | none | `http://localhost:11434` |
| `lmstudio`    | LM Studio (local)          | OpenAI-compatible       | none | `http://localhost:1234` |
| `llamacpp`    | llama.cpp server (local)   | OpenAI-compatible       | none | `http://localhost:8080` |
| `vllm`        | vLLM (local/self-hosted)   | OpenAI-compatible       | optional | `http://localhost:8000` |
| `llama`       | llama.cpp **in-process**   | FFI (no HTTP)           | none | — (needs `model_path`) |
| `custom`      | any OpenAI-compatible API  | OpenAI Chat Completions | optional | (set `base_url`) |
| `mock`        | deterministic stub         | —                       | none | — |

Unknown providers fall back to the OpenAI Chat Completions protocol, so any new
vendor exposing `/v1/chat/completions` works via `provider: "custom"` plus a
`base_url`.

## Configuration shape

```jsonc
{
  "provider": "openai",            // see table above
  "model":    "gpt-4o",            // required (model_path for in-process llama)
  "api_key":  "sk-...",            // API key or OAuth access token
  "base_url": "https://...",       // optional — provider default applies
  "auth_method": "api_key",        // google only: "api_key" | "bearer"
  "embed_model": "text-embedding-3-small", // optional
  "timeout_seconds": 120,          // optional
  "max_context": 128000            // optional
}
```

`backend` is accepted as an alias for `provider` (older configs).

## C ABI

```c
/* Set the default backend at runtime (e.g. after the user signs in). */
am_status_t am_configure_llm(AgentManager* mgr, const char* llm_config_json);

/* Per-agent override — pass an "llm" object in the spawn config. */
am_status_t am_spawn_agent(AgentManager* mgr, const char* config_json,
                           char* out_id_buf, size_t out_size);
```

Resolution order for an agent's model: **per-agent `llm`** → **manager default
LLM** (from `am_create`'s `llm` key or the most recent `am_configure_llm`).

### Mixed-provider agent groups

Because each agent can carry its own `llm`, a single group can mix providers —
e.g. a Claude planner delegating to a GPT-4o coder and a local Llama reviewer.
They coordinate through the engine's pipes, messages, and blackboard exactly as
same-provider agents do; only the model behind each agent differs.

```jsonc
// planner
{"name":"planner","llm":{"provider":"anthropic","model":"claude-sonnet-4-6","api_key":"..."}}
// coder
{"name":"coder","llm":{"provider":"openai","model":"gpt-4o","api_key":"..."}}
// reviewer (local, no key)
{"name":"reviewer","llm":{"provider":"ollama","model":"llama3.1","base_url":"http://localhost:11434"}}
```

## Building

The hosted-API backends are opt-in (they pull in cpp-httplib + OpenSSL):

```bash
cmake .. -DAGENT_ENABLE_API_LLM=ON \
         -DHTTPLIB_INCLUDE_DIR=/path/to/cpp-httplib
make -j$(nproc)
```

- `AGENT_ENABLE_API_LLM=ON` builds `aimodel_api.cpp` (OpenAI/Anthropic/Gemini/
  Ollama + all OpenAI-compatible vendors) and defines `AGENT_HAS_API_LLM`.
- OpenSSL is auto-detected; without it only `http://` providers (Ollama, LM
  Studio, llama.cpp, vLLM) are reachable — `https://` hosted providers need it.
- `AGENT_ENABLE_LLAMA=ON` adds the in-process `llama` provider (no HTTP).
- When the engine is built without `AGENT_ENABLE_API_LLM`, requesting a hosted
  provider returns `AM_ERROR_INVALID_ARG` with a clear message; the engine
  still runs on `mock` or in-process `llama`.

## Credentials & safety

API keys and OAuth tokens are read by the C++ HTTP layer and attached to each
request's headers (or query string for Gemini API-key auth). They are **never**
placed into prompts, tool arguments, or model-visible context, and are not
logged.
