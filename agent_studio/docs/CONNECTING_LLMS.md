# Connecting LLMs in Agent Studio

Agent Studio (web and desktop) can connect to hosted and local LLMs and use
them to power agents. This covers how to add providers, how authentication
works, and how agents on different providers connect together.

## Adding a provider

Open **Settings → Model Providers → Add**. Pick a provider:

- **Hosted:** Claude, ChatGPT, Gemini, Groq, Mistral, DeepSeek, Grok,
  OpenRouter, Together AI.
- **Local:** Ollama, LM Studio, llama.cpp, vLLM, or any custom
  OpenAI-compatible server.

Enter the **name**, **base URL** (a sensible default is filled in), and
authenticate (below). On connect, the studio lists the provider's models.

## Authentication methods

| Method | Providers | How |
|--------|-----------|-----|
| **API key** | all hosted | Paste the key from the provider's console. |
| **Sign in with Google** | Gemini | OAuth 2.0 + PKCE; needs a Google Cloud OAuth client ID. |
| **OAuth bearer token** | Gemini | Paste an access token obtained elsewhere. |
| **None** | local | Local servers need no credential. |

### Sign in with Google (Gemini)

1. In Google Cloud, create an **OAuth client ID**:
   - Desktop app: type *Desktop* (used by the desktop build's loopback flow).
   - Web app: add `<your-origin>/google_oauth.html` as an authorized redirect
     URI (used by the web build's popup flow).
2. In the Add Provider dialog, choose **Gemini**, paste the **client ID**, and
   click **Sign in with Google**. A browser window opens for consent.
3. On success the access token is stored and used as the Gemini credential.

The flow uses PKCE (no client secret in the browser). Desktop captures the
redirect on a localhost port; web uses a popup + `postMessage`.

## Local LLMs

| Server | Start command | Base URL |
|--------|---------------|----------|
| Ollama | `ollama serve` | `http://localhost:11434` |
| LM Studio | start the local server in the app | `http://localhost:1234` |
| llama.cpp | `./llama-server -m model.gguf` | `http://localhost:8080` |
| vLLM | `vllm serve <model>` | `http://localhost:8000` |

No API key is required. Make sure the server is running before connecting.

## Using providers with agents

- **Direct chat:** When an agent is bound to a connected provider, the studio
  calls that provider's API directly for chat.
- **Per-agent model:** Each agent's **model** (in the agent builder) selects the
  provider + model used to run it.
- **Engine default:** Click the ⚡ icon on a provider row to make it the default
  for engine-backed agents. The first provider you connect becomes the default
  automatically. The active one shows an **ENGINE** badge.

## Connecting different LLM agents together

Agents on different providers interoperate. A group can mix, say, a Claude
planner, a ChatGPT coder, and a local Llama reviewer; they coordinate through
the engine (hierarchy/parallel/pipeline/graph formations, messages, shared
blackboard) regardless of the model behind each one. The studio passes each
agent's provider/model to the engine as a per-agent LLM config; see
`agent/docs/LLM_PROVIDERS.md` for the engine-side detail.

## Web vs desktop notes

- **Desktop** talks to the C++ engine via FFI and can also call provider APIs
  directly; Google sign-in uses the loopback flow.
- **Web** calls provider APIs directly from the browser. Anthropic requires the
  `anthropic-dangerous-direct-browser-access` header (sent automatically);
  Google and most OpenAI-compatible vendors allow browser calls. Some vendors
  block cross-origin browser requests — use the desktop build or a proxy for
  those.

## Security

API keys and OAuth tokens stay between the studio and the provider/engine. They
are never written into prompts or model-visible context.
