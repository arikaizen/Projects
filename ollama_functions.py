#!/usr/bin/env python3
"""
ollama_functions.py
─────────────────────────────────────────────────────────────────────────────
Class-based wrapper around a local Ollama model.
Each OllamaChat instance is an independent conversation — create as many as
you need in the same process without them interfering with each other.

USAGE IN YOUR MAIN FILE:
    from ollama_functions import OllamaChat, generate, embed, similarity, search

    # --- Single stateless prompt (no memory) ---
    response = generate("write a CUDA kernel")
    print(response)

    # --- One conversation ---
    conv = OllamaChat()
    reply1 = conv.chat("what is warp divergence?")
    reply2 = conv.chat("show me a code example")   # model remembers previous message

    # --- Multiple independent conversations at the same time ---
    conv1 = OllamaChat()
    conv2 = OllamaChat()
    conv1.chat("explain CUDA streams")
    conv2.chat("explain Python GIL")               # completely separate history

    # --- Semantic utilities (stateless) ---
    score   = similarity("CUDA kernel", "GPU function")
    vector  = embed("shared memory optimization")
    results = search("memory allocation", ["file1.cu", "file2.cu"], ["code1", "code2"])

INSTALL DEPENDENCIES:
    pip install ollama numpy
"""

import json
import os
from datetime import datetime

import numpy as np
import ollama

# ─────────────────────────────────────────────────────────────────────────────
# CONFIGURATION — change these to match your setup
# ─────────────────────────────────────────────────────────────────────────────
MODEL = "qwen2.5-coder:14b"
SYSTEM_PROMPT = (
    "You are an expert C, C++, Python and CUDA programmer. "
    "Be concise and precise. When writing code always include comments."
)

# Shared embedding cache across all OllamaChat instances and embed() calls.
# Maps text → numpy vector so the same text is never embedded twice.
_embedding_cache: dict = {}


# ─────────────────────────────────────────────────────────────────────────────
# STANDALONE FUNCTION 1 — generate()
# Single prompt → single response. Completely stateless, no conversation.
# ─────────────────────────────────────────────────────────────────────────────
def generate(prompt, stream=False, temperature=0.7, max_tokens=2048):
    """
    Send a single prompt and get one response back.
    Each call is completely independent — no memory of previous calls.

    Args:
        prompt      (str)   — your question or instruction
        stream      (bool)  — True: print tokens as they arrive
                              False: return full response as a string (default)
        temperature (float) — 0.0 = deterministic
                              0.7 = balanced (default)
                              2.0 = very creative (max)
        max_tokens  (int)   — maximum length of the response, must be >= 1

    Returns:
        str — the model's full response

    Raises:
        ValueError        — prompt is empty or whitespace only
        ValueError        — temperature is outside 0.0–2.0
        ValueError        — max_tokens is less than 1
        ConnectionError   — Ollama server is not running
        ollama.ResponseError — model not found or API returned an error
        RuntimeError      — API returned a response with no text content

    Example:
        response = generate("write a CUDA kernel for vector addition")
        print(response)

        # Stream tokens to the terminal as they arrive
        generate("explain shared memory", stream=True)
    """
    if not prompt or not prompt.strip():
        raise ValueError("prompt cannot be empty")

    if not (0.0 <= temperature <= 2.0):
        raise ValueError(f"temperature must be between 0.0 and 2.0, got {temperature}")

    if max_tokens < 1:
        raise ValueError(f"max_tokens must be at least 1, got {max_tokens}")

    options = {"temperature": temperature, "num_predict": max_tokens}

    try:
        if stream:
            full_response = ""
            chunks = ollama.generate(
                model=MODEL, prompt=prompt, stream=True, options=options
            )
            for chunk in chunks:
                token = chunk["response"]
                print(token, end="", flush=True)
                full_response += token
            print()
            return full_response
        else:
            response = ollama.generate(
                model=MODEL, prompt=prompt, stream=False, options=options
            )
            text = response["response"]
            if text is None:
                raise RuntimeError("Ollama returned a response with no text content")
            return text

    except ConnectionError:
        # Re-raise with a clear message so the caller knows to start Ollama
        raise ConnectionError(
            "Could not reach the Ollama server. "
            "Make sure Ollama is running: https://ollama.com/download"
        )
    except ollama.ResponseError as e:
        # Re-raise with context so the caller knows which model failed
        raise ollama.ResponseError(
            f"Ollama API error in generate() using model '{MODEL}': {e}"
        )


# ─────────────────────────────────────────────────────────────────────────────
# STANDALONE FUNCTION 2 — embed()
# Convert text to a vector of numbers representing its meaning.
# ─────────────────────────────────────────────────────────────────────────────
def embed(text, use_cache=True):
    """
    Convert text to a numeric vector that represents its meaning.

    The vector is a numpy array of floats (e.g. 5120 numbers for a 14b model).
    The same text always produces the same vector.
    Texts with similar meanings produce similar vectors.
    This is the building block for similarity() and search().

    Args:
        text      (str)  — any text to convert
        use_cache (bool) — True: reuse vectors for text seen before (default)
                           False: always call the API (useful for testing)

    Returns:
        numpy.ndarray — 1-D array of floats

    Raises:
        ValueError        — text is empty or whitespace only
        ConnectionError   — Ollama server is not running
        ollama.ResponseError — model not found or API returned an error
        RuntimeError      — API returned an empty embedding vector

    Example:
        vector = embed("CUDA kernel for matrix multiply")
        print(f"Vector length: {len(vector)}")    # e.g. 5120
        print(f"First 3 values: {vector[:3]}")    # e.g. [ 0.23 -0.87  0.45]
    """
    if not text or not text.strip():
        raise ValueError("text cannot be empty")

    # Return the cached result if we already computed this text
    if use_cache and text in _embedding_cache:
        return _embedding_cache[text]

    try:
        response = ollama.embeddings(model=MODEL, prompt=text)
    except ConnectionError:
        raise ConnectionError(
            "Could not reach the Ollama server. "
            "Make sure Ollama is running: https://ollama.com/download"
        )
    except ollama.ResponseError as e:
        raise ollama.ResponseError(
            f"Ollama API error in embed() using model '{MODEL}': {e}"
        )

    embedding = response.get("embedding")
    if not embedding:
        raise RuntimeError(
            f"Ollama returned an empty embedding for model '{MODEL}'. "
            "Make sure the model supports embeddings."
        )

    vector = np.array(embedding)

    if use_cache:
        _embedding_cache[text] = vector

    return vector


# ─────────────────────────────────────────────────────────────────────────────
# STANDALONE FUNCTION 3 — similarity()
# Compare two texts by meaning. Returns a score from -1.0 to 1.0.
# ─────────────────────────────────────────────────────────────────────────────
def similarity(text_a, text_b):
    """
    Compare two pieces of text and return how similar their meaning is.

    Uses cosine similarity on their embedding vectors.
    Returns a float between -1.0 and 1.0 (in practice, usually 0.0 – 1.0).

    Score guide:
        1.00 = identical meaning
        0.90 = very similar
        0.70 = somewhat related
        0.50 = loosely related
        0.10 = unrelated

    Args:
        text_a (str) — first text
        text_b (str) — second text

    Returns:
        float — similarity score, rounded to 4 decimal places

    Raises:
        ValueError        — either text is empty or whitespace only
        RuntimeError      — a vector has zero magnitude (cannot compute cosine similarity)
        ConnectionError   — Ollama server is not running
        ollama.ResponseError — model not found or API returned an error

    Example:
        score = similarity("CUDA kernel", "GPU function")
        print(score)   # e.g. 0.91 — very similar

        score = similarity("CUDA kernel", "baking a cake")
        print(score)   # e.g. 0.12 — unrelated
    """
    vector_a = embed(text_a)
    vector_b = embed(text_b)

    norm_a = np.linalg.norm(vector_a)
    norm_b = np.linalg.norm(vector_b)

    # Guard against division by zero — a zero vector has no direction so
    # cosine similarity is undefined
    if norm_a == 0:
        raise RuntimeError(
            f"The embedding for text_a has zero magnitude — "
            f"cosine similarity is undefined. Text: '{text_a[:60]}'"
        )
    if norm_b == 0:
        raise RuntimeError(
            f"The embedding for text_b has zero magnitude — "
            f"cosine similarity is undefined. Text: '{text_b[:60]}'"
        )

    # Cosine similarity: dot product / (magnitude_a × magnitude_b)
    # Result is between -1.0 (opposite) and 1.0 (identical direction)
    dot   = np.dot(vector_a, vector_b)
    score = dot / (norm_a * norm_b)

    return round(float(score), 4)


# ─────────────────────────────────────────────────────────────────────────────
# STANDALONE FUNCTION 4 — search()
# Find the most semantically relevant items from a list.
# ─────────────────────────────────────────────────────────────────────────────
def search(query, labels, texts, top_n=3):
    """
    Find the most semantically similar items to a query from a list.

    Does NOT search for exact words — finds the closest meaning using embeddings.

    Args:
        query  (str)       — what you are looking for (must not be empty)
        labels (list[str]) — display names for each item, e.g. ["file1.cu", "file2.cu"]
        texts  (list[str]) — content of each item,       e.g. [code1, code2]
        top_n  (int)       — how many top results to return, must be >= 1 (default 3)

    Returns:
        list of (score: float, label: str) tuples, sorted best-first

    Raises:
        ValueError        — query is empty or whitespace only
        ValueError        — labels and texts have different lengths
        ValueError        — labels and texts are empty
        ValueError        — top_n is less than 1
        ConnectionError   — Ollama server is not running
        ollama.ResponseError — model not found or API returned an error

    Example:
        labels  = ["kernel.cu",    "utils.h",         "main.cpp"]
        texts   = ["cuda code...", "utility code...", "main code..."]

        results = search("memory allocation", labels, texts)
        for score, label in results:
            print(f"{score:.3f} — {label}")
    """
    if not query or not query.strip():
        raise ValueError("query cannot be empty")

    if len(labels) != len(texts):
        raise ValueError(
            f"labels ({len(labels)}) and texts ({len(texts)}) must be the same length"
        )
    if not labels:
        raise ValueError("labels and texts cannot be empty")

    if top_n < 1:
        raise ValueError(f"top_n must be at least 1, got {top_n}")

    query_vec = embed(query)

    results = []
    for label, text in zip(labels, texts):
        item_vec = embed(text)
        norm_q = np.linalg.norm(query_vec)
        norm_i = np.linalg.norm(item_vec)
        # Skip items whose vector has zero magnitude — cosine similarity undefined
        if norm_q == 0 or norm_i == 0:
            score = 0.0
        else:
            score = float(np.dot(query_vec, item_vec) / (norm_q * norm_i))
        results.append((round(score, 4), label))

    # Sort highest score first, then return the requested number of results
    results.sort(key=lambda x: x[0], reverse=True)
    return results[:top_n]


# ─────────────────────────────────────────────────────────────────────────────
# CLASS — OllamaChat
# Stateful conversation with full history. Create one instance per conversation.
# ─────────────────────────────────────────────────────────────────────────────
class OllamaChat:
    """
    A stateful conversation with the Ollama model.

    Each instance holds its own independent message history, so you can run
    multiple conversations in the same process without them interfering.

    Example:
        # Single conversation
        conv = OllamaChat()
        reply = conv.chat("what is warp divergence?")
        reply = conv.chat("show me a code example")   # model remembers context

        # Two independent conversations running side by side
        conv1 = OllamaChat()
        conv2 = OllamaChat()
        conv1.chat("explain CUDA streams")
        conv2.chat("explain Python GIL")   # completely separate history

        # Custom model and system prompt
        conv = OllamaChat(model="llama3:8b", system_prompt="You are a helpful assistant.")
    """

    def __init__(self, model=MODEL, system_prompt=SYSTEM_PROMPT):
        """
        Create a new, empty conversation.

        Args:
            model         (str) — Ollama model name, must not be empty
            system_prompt (str) — instructions given to the model at the start
                                  of every conversation, must not be empty

        Raises:
            ValueError — model is empty or whitespace only
            ValueError — system_prompt is empty or whitespace only

        Example:
            conv = OllamaChat()
            conv = OllamaChat(model="llama3:8b", system_prompt="You are a pirate.")
        """
        if not model or not str(model).strip():
            raise ValueError("model name cannot be empty")

        if not system_prompt or not str(system_prompt).strip():
            raise ValueError("system_prompt cannot be empty")

        self.model = model
        self._history = [{"role": "system", "content": system_prompt}]
        self._title = None

    # ── Conversation ──────────────────────────────────────────────────────────

    def chat(self, message, stream=False, temperature=0.7, max_tokens=2048):
        """
        Send a message and get a reply. The model remembers all previous messages.

        Each call appends both the user message and the assistant reply to the
        internal history, so follow-up questions have full context.
        If the API call fails, the user message is rolled back from the history
        so the conversation stays in a consistent state.

        Args:
            message     (str)   — your message, must not be empty
            stream      (bool)  — True: print tokens as they arrive
                                  False: return full reply as string (default)
            temperature (float) — 0.0 = deterministic / 0.7 = balanced / 2.0 = max
            max_tokens  (int)   — maximum response length, must be >= 1

        Returns:
            str — the model's full reply

        Raises:
            ValueError        — message is empty or whitespace only
            ValueError        — temperature is outside 0.0–2.0
            ValueError        — max_tokens is less than 1
            ConnectionError   — Ollama server is not running
            ollama.ResponseError — model not found or API returned an error
            RuntimeError      — API returned a reply with no text content

        Example:
            conv = OllamaChat()
            reply = conv.chat("what is warp divergence?")
            reply = conv.chat("show me a code example")   # model remembers context
            reply = conv.chat("now optimize that code")
        """
        if not message or not message.strip():
            raise ValueError("message cannot be empty")

        if not (0.0 <= temperature <= 2.0):
            raise ValueError(
                f"temperature must be between 0.0 and 2.0, got {temperature}"
            )

        if max_tokens < 1:
            raise ValueError(f"max_tokens must be at least 1, got {max_tokens}")

        # Track whether this is the first real message (used for auto-title below)
        is_first_message = len(self._history) == 1

        # Append user turn before calling the API so the full context is sent
        self._history.append({"role": "user", "content": message})

        options = {"temperature": temperature, "num_predict": max_tokens}

        try:
            if stream:
                full_reply = ""
                chunks = ollama.chat(
                    model=self.model,
                    messages=self._history,
                    stream=True,
                    options=options,
                )
                for chunk in chunks:
                    token = chunk["message"]["content"]
                    print(token, end="", flush=True)
                    full_reply += token
                print()
            else:
                response = ollama.chat(
                    model=self.model,
                    messages=self._history,
                    stream=False,
                    options=options,
                )
                full_reply = response["message"]["content"]
                if full_reply is None:
                    raise RuntimeError(
                        "Ollama returned a reply with no text content"
                    )

        except ConnectionError:
            # Roll back the user message so history stays consistent
            self._history.pop()
            raise ConnectionError(
                "Could not reach the Ollama server. "
                "Make sure Ollama is running: https://ollama.com/download"
            )
        except ollama.ResponseError as e:
            # Roll back the user message so history stays consistent
            self._history.pop()
            raise ollama.ResponseError(
                f"Ollama API error in chat() using model '{self.model}': {e}"
            )
        except RuntimeError:
            # Roll back the user message so history stays consistent
            self._history.pop()
            raise

        # Append assistant turn so the next call has full context
        self._history.append({"role": "assistant", "content": full_reply})

        # Auto-generate a short title from the very first user message
        if is_first_message and self._title is None:
            self._title = self._generate_title(message)

        return full_reply

    def clear_history(self):
        """
        Reset the conversation — wipe all messages and the title.

        The system prompt is preserved so the model still knows its role,
        but all previous exchanges are forgotten.

        Example:
            conv.chat("what is CUDA?")
            conv.clear_history()
            conv.chat("what did I just ask?")   # model has no memory of it
        """
        # Preserve the original system prompt when resetting
        system_prompt = self._history[0]["content"]
        self._history = [{"role": "system", "content": system_prompt}]
        self._title = None

    # ── History persistence ───────────────────────────────────────────────────

    def get_history(self):
        """
        Return a copy of the current conversation history.

        Returns a copy, not a reference, so modifying the returned list does
        not affect the internal state of this conversation.

        Returns:
            list[dict] — list of {"role": ..., "content": ...} message dicts

        Example:
            for msg in conv.get_history():
                print(f"{msg['role']}: {msg['content'][:80]}")
        """
        return self._history.copy()

    def save_history(self, filepath=None):
        """
        Save the current conversation history to a JSON file on disk.

        The file can be reloaded later with load_history() to continue the
        conversation from where it left off.

        Args:
            filepath (str) — where to save
                             default: ~/conversations/session_TIMESTAMP.json

        Returns:
            str — the path where the file was saved

        Raises:
            OSError — filepath is not writable or the directory cannot be created

        Example:
            path = conv.save_history()
            print(f"Saved to {path}")

            # Save to a specific location
            conv.save_history("~/my_session.json")
        """
        if filepath is None:
            save_dir = os.path.expanduser("~/conversations")
            try:
                os.makedirs(save_dir, exist_ok=True)
            except OSError as e:
                raise OSError(
                    f"Could not create conversations directory '{save_dir}': {e}"
                )
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filepath = f"{save_dir}/session_{timestamp}.json"

        filepath = os.path.expanduser(filepath)
        payload = {"title": self._title, "messages": self._history}

        try:
            with open(filepath, "w") as f:
                json.dump(payload, f, indent=2)
        except OSError as e:
            raise OSError(
                f"Could not write history to '{filepath}': {e}"
            )

        return filepath

    def load_history(self, filepath):
        """
        Load a previously saved conversation from disk.

        After loading, the model will have full memory of that saved session
        when you call chat(). Any current history in this instance is replaced.

        Supports both the current format {"title":..., "messages":[...]} and
        the old plain-list format from earlier versions.

        Args:
            filepath (str) — path to the saved JSON file

        Raises:
            FileNotFoundError    — file does not exist
            json.JSONDecodeError — file is not valid JSON
            ValueError           — file structure is not a valid history format

        Example:
            conv.load_history("~/conversations/session_20260320.json")
            conv.chat("continue where we left off")
        """
        filepath = os.path.expanduser(filepath)

        if not os.path.exists(filepath):
            raise FileNotFoundError(f"History file not found: {filepath}")

        with open(filepath) as f:
            data = json.load(f)

        # Support both {"title":..., "messages":[...]} and the old plain list format
        if isinstance(data, list):
            messages = data
            title = None
        elif isinstance(data, dict):
            if "messages" not in data:
                raise ValueError(
                    f"Invalid history file '{filepath}': "
                    "missing required 'messages' key"
                )
            messages = data["messages"]
            title = data.get("title")
        else:
            raise ValueError(
                f"Invalid history file '{filepath}': "
                "expected a JSON object or list, got "
                f"{type(data).__name__}"
            )

        if not isinstance(messages, list):
            raise ValueError(
                f"Invalid history file '{filepath}': "
                "'messages' must be a list"
            )

        # Validate each message has the required role and content fields
        for i, msg in enumerate(messages):
            if not isinstance(msg, dict):
                raise ValueError(
                    f"Invalid history file '{filepath}': "
                    f"message at index {i} is not a dict"
                )
            if "role" not in msg or "content" not in msg:
                raise ValueError(
                    f"Invalid history file '{filepath}': "
                    f"message at index {i} is missing 'role' or 'content'"
                )

        self._history = messages
        self._title = title

    # ── Title ─────────────────────────────────────────────────────────────────

    def get_title(self):
        """
        Return the current conversation title.

        The title is auto-generated from the first chat() message, or can be
        set manually with set_title().

        Returns:
            str or None — the title, or None if no conversation has started yet

        Example:
            conv.chat("explain warp divergence")
            print(conv.get_title())   # e.g. "Warp Divergence in CUDA"
        """
        return self._title

    def set_title(self, title):
        """
        Set or overwrite the conversation title manually.

        Args:
            title (str) — new title; leading/trailing whitespace is stripped

        Raises:
            ValueError — title is empty or whitespace only

        Example:
            conv.set_title("CUDA Shared Memory Session")
            print(conv.get_title())   # "CUDA Shared Memory Session"
        """
        if not title or not title.strip():
            raise ValueError("title cannot be empty")
        self._title = title.strip()

    # ── Private helpers ───────────────────────────────────────────────────────

    def _generate_title(self, first_message):
        """
        Ask the model for a short title based on the first user message.
        Falls back to a truncated version of the message if the API fails
        for any reason — title generation must never crash a conversation.

        Args:
            first_message (str) — the first user message in the conversation

        Returns:
            str — a short title (4-6 words), no surrounding quotes or punctuation
        """
        prompt = (
            f'Give a short title (4-6 words) for a conversation that starts with:\n'
            f'"{first_message}"\n'
            f'Reply with the title only — no quotes, no punctuation at the end.'
        )
        try:
            title = generate(prompt, temperature=0.3, max_tokens=20)
            return title.strip().strip('"').strip("'")
        except Exception:
            # Fall back to a truncated version of the first message so the
            # conversation always gets some kind of title
            return first_message[:60].strip()
