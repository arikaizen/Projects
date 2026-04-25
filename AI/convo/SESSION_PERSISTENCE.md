## Session persistence (history + KV cache)

This project persists **two different kinds of conversation state**:

- **Prompt/history state (text)**: stored as JSON via `AIConvo::SaveHistory()` / `LoadHistory()`.
- **Inference state (binary)**: stored as a llama.cpp context snapshot via `AIConvo::SaveState()` / `LoadState()`.

The binary snapshot contains the conversation context's internal "memory", which includes the
**attention KV cache** and other runtime state needed to continue generation without re-decoding
the full prompt.

### File layout

When saving a full session directory with `ConvoManager::SaveSession(dir)`:

- `dir/manifest.json`
- `dir/convos/model_<modelId>/convo_<convoId>.json`
- `dir/convos/model_<modelId>/convo_<convoId>.state.bin`

### What gets saved

- **Per model**
  - `model_path`
  - `context_size`, `thread_count`
  - `active_conversation`
- **Per conversation**
  - history JSON (`messages[]` with `role` + `content`, and optional `title`)
  - prompt assembly config:
    - `system_prompt_file` (optional)
    - `recent_turns_window` (int, user/assistant pairs)
    - `summary` (hidden, AI-generated)
  - llama.cpp state snapshot (`.state.bin`)
  - whether it was marked `closed`

### Load behavior and fallback

`ConvoManager::LoadSession(dir)` clears the current in-memory session and recreates:

- models (new `AIModel` instances with the stored parameters)
- conversations (new `AIConvo` contexts)
- loads history JSON into each conversation
- attempts to restore the state snapshot

If the snapshot restore fails (missing file or incompatible state), the conversation still loads
**history-only**. The next `Chat()` will rebuild the KV cache from the full prompt (slower, but correct).

### Prompt flow (how it’s maintained)

1. `AIConvo::_history` stores structured messages `{role, content}`.
2. Each `AIConvo::Chat(user_message)` appends the user message to `_history`.
3. `AIConvo::BuildPrompt()` assembles a prompt from:
   - the system prompt loaded from `system_prompt_file` (if configured), otherwise the inline system prompt
   - the hidden `summary` (if present)
   - the last `recent_turns_window` user/assistant pairs from `_history`
4. The assembled message list is serialized into one prompt string using either:
   - the model's built-in chat template (`llama_chat_apply_template`), or
   - a plain `role: content` fallback.
5. The prompt string is tokenized and decoded.
6. The assistant reply is appended to `_history`.
7. A hidden summarization call updates `summary` for the next turn (best-effort).

### KV cache flow (current design)

Inside `AIConvo::RunChat(...)`:

1. `ClearKvCache()` wipes the llama context memory (KV cache).
2. The full prompt tokens are decoded in one batch (`llama_decode(prompt_batch)`).
3. Reply tokens are generated; each new token is decoded, extending the KV cache.

This means KV cache is **used during a single turn**, but is **not retained across turns**
because it is cleared at the start of every turn.

### Sidecar state for single-conversation saves

`ConvoManager::Save(model_id, convo_id, path)` writes:

- history to `path` (or an auto filename if empty)
- state to `<history_path>.state.bin`

## Scheduled saves (CLI)

The CLI includes a cooperative scheduler that runs inside the main input loop
(single-threaded). This avoids concurrency issues while still allowing periodic
saves.

Commands:

- `/sched list`
- `/sched add session <seconds> [dir]`
- `/sched add active <seconds> [path]`
- `/sched add all <seconds>`
- `/sched rm <id>`
- `/sched enable <id>`
- `/sched disable <id>`
- `/sched run <id>`

Notes:
- Tasks run **best-effort**: errors are recorded and shown in `/sched list`.

