#!/usr/bin/env python3
"""
ollama_functions.py
─────────────────────────────────────────────────────────────────────────────
Standalone functions for talking to a local Ollama model.
Copy this file into your project and import the functions you need.
USAGE IN YOUR MAIN FILE:
    from ollama_functions import generate, chat, similarity, embed, search, get_title, set_title
    # Single prompt
    response = generate("write a CUDA kernel")
    print(response)
    # Conversation
    reply1 = chat("what is warp divergence?")
    reply2 = chat("show me a code example")   # remembers previous message
    # Compare meaning
    score = similarity("CUDA kernel", "GPU function")
    # Convert to vector
    vector = embed("shared memory optimization")
    # Search a list by meaning
    results = search("memory allocation", ["file1.cu", "file2.cu"], ["code1", "code2"])
INSTALL DEPENDENCIES:
    pip install ollama numpy
"""
import ollama
import numpy as np
import json
import os
from datetime import datetime
# ─────────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# Change these to match your setup
# ─────────────────────────────────────────────────────────────────────────────
MODEL = "qwen2.5-coder:14b"
SYSTEM_PROMPT = (
    "You are an expert C, C++, Python and CUDA programmer. "
    "Be concise and precise. When writing code always include comments."
)
# Internal state — do not modify directly
# Use clear_history() to reset, save_history() to save
_conversation_history = [
    {"role": "system", "content": SYSTEM_PROMPT}
]
_conversation_title = None
_embedding_cache = {}
# ─────────────────────────────────────────────────────────────────────────────
# FUNCTION 1 — generate()
# Single prompt → single response. No conversation history.
# ─────────────────────────────────────────────────────────────────────────────
def generate(prompt, stream=False, temperature=0.7, max_tokens=2048):
    """
    Send a single prompt and get a response back.
    Each call is completely independent — no memory of previous calls.
    Args:
        prompt      (str)   — your question or instruction
        stream      (bool)  — True: print tokens as they arrive
                              False: return full response as string (default)
        temperature (float) — 0.0 = deterministic
                              0.7 = balanced (default)
                              1.5 = creative
        max_tokens  (int)   — maximum length of response
    Returns:
        str — the model's full response
    Example:
        response = generate("write a CUDA kernel for vector addition")
        print(response)
        generate("explain shared memory", stream=True)
    """
    if not prompt or not prompt.strip():
        raise ValueError("prompt cannot be empty")
    options = {
        "temperature": temperature,
        "num_predict": max_tokens
    }
    if stream:
        full_response = ""
        chunks = ollama.generate(
            model=MODEL,
            prompt=prompt,
            stream=True,
            options=options
        )
        for chunk in chunks:
            token = chunk['response']
            print(token, end='', flush=True)
            full_response += token
        print()
        return full_response
    else:
        response = ollama.generate(
            model=MODEL,
            prompt=prompt,
            stream=False,
            options=options
        )
        return response['response']
# ─────────────────────────────────────────────────────────────────────────────
# FUNCTION 2 — chat()
# Send a message in a conversation. Model remembers all previous messages.
# ─────────────────────────────────────────────────────────────────────────────
def chat(message, stream=False, temperature=0.7, max_tokens=2048):
    """
    Send a message in a conversation.
    The model remembers all previous messages sent with chat() in this session.
    Each call adds to the conversation history automatically.
    Args:
        message     (str)   — your message
        stream      (bool)  — True: print tokens as they arrive
                              False: return full response as string (default)
        temperature (float) — 0.0 = deterministic
                              0.7 = balanced (default)
                              1.5 = creative
        max_tokens  (int)   — maximum length of response
    Returns:
        str — the model's full response
    Example:
        reply = chat("what is warp divergence?")
        print(reply)
        reply = chat("show me a code example")   # model remembers previous message
        reply = chat("now optimize that code")   # model remembers both previous messages
    """
    global _conversation_history, _conversation_title
    if not message or not message.strip():
        raise ValueError("message cannot be empty")
    is_first_message = len(_conversation_history) == 1  # only system prompt so far
    # Add user message to history
    _conversation_history.append({
        "role": "user",
        "content": message
    })
    options = {
        "temperature": temperature,
        "num_predict": max_tokens
    }
    if stream:
        full_reply = ""
        chunks = ollama.chat(
            model=MODEL,
            messages=_conversation_history,
            stream=True,
            options=options
        )
        for chunk in chunks:
            token = chunk['message']['content']
            print(token, end='', flush=True)
            full_reply += token
        print()
    else:
        response = ollama.chat(
            model=MODEL,
            messages=_conversation_history,
            stream=False,
            options=options
        )
        full_reply = response['message']['content']
    # Add model reply to history so next call remembers this exchange
    _conversation_history.append({
        "role": "assistant",
        "content": full_reply
    })
    # Auto-generate a title from the first message
    if is_first_message and _conversation_title is None:
        _conversation_title = _generate_title(message)
    return full_reply
def clear_history():
    """
    Wipe the conversation history and title, and start fresh.
    Keeps the system prompt but removes all messages.
    Example:
        chat("what is CUDA?")
        clear_history()
        chat("what did I just ask?")   # model has no memory of it
    """
    global _conversation_history, _conversation_title
    _conversation_history = [
        {"role": "system", "content": SYSTEM_PROMPT}
    ]
    _conversation_title = None
def _generate_title(first_message):
    """
    Ask the model for a short title based on the first user message.
    Returns a plain string, no quotes or punctuation.
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
        # Fall back to a truncated version of the first message
        return first_message[:60].strip()
def get_title():
    """
    Return the current conversation title.
    Auto-generated from the first chat() message, or set manually with set_title().
    Returns:
        str or None — title string, or None if no conversation has started yet
    Example:
        chat("explain warp divergence")
        print(get_title())   # "Warp Divergence in CUDA"
    """
    return _conversation_title
def set_title(title):
    """
    Set or overwrite the conversation title manually.
    Args:
        title (str) — new title
    Raises:
        ValueError — if title is empty or whitespace
    Example:
        set_title("CUDA Shared Memory Session")
        print(get_title())   # "CUDA Shared Memory Session"
    """
    global _conversation_title
    if not title or not title.strip():
        raise ValueError("title cannot be empty")
    _conversation_title = title.strip()
def save_history(filepath=None):
    """
    Save the current conversation history to a JSON file on disk.
    Args:
        filepath (str) — where to save
                         default: ~/conversations/session_TIMESTAMP.json
    Returns:
        str — the path where the file was saved
    Example:
        path = save_history()
        print(f"Saved to {path}")
        save_history("~/my_session.json")
    """
    if filepath is None:
        save_dir = os.path.expanduser("~/conversations")
        os.makedirs(save_dir, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filepath = f"{save_dir}/session_{timestamp}.json"
    filepath = os.path.expanduser(filepath)
    payload = {
        "title": _conversation_title,
        "messages": _conversation_history,
    }
    with open(filepath, "w") as f:
        json.dump(payload, f, indent=2)
    return filepath
def load_history(filepath):
    """
    Load a previously saved conversation history from disk.
    The model will have full memory of that session when you call chat().
    Args:
        filepath (str) — path to the saved JSON file
    Example:
        load_history("~/conversations/session_20260320.json")
        chat("continue where we left off")
    """
    global _conversation_history, _conversation_title
    filepath = os.path.expanduser(filepath)
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"History file not found: {filepath}")
    with open(filepath) as f:
        data = json.load(f)
    # Support both new format {"title":..., "messages":[...]} and old list format
    if isinstance(data, list):
        _conversation_history = data
        _conversation_title = None
    else:
        _conversation_history = data["messages"]
        _conversation_title = data.get("title")
def get_history():
    """
    Return the current conversation history as a list of message dicts.
    Returns:
        list — conversation history
    Example:
        history = get_history()
        for message in history:
            print(f"{message['role']}: {message['content'][:80]}")
    """
    return _conversation_history.copy()
# ─────────────────────────────────────────────────────────────────────────────
# FUNCTION 3 — embed()
# Convert text to a vector of numbers representing its meaning.
# ─────────────────────────────────────────────────────────────────────────────
def embed(text, use_cache=True):
    """
    Convert text to a vector of numbers that represents its meaning.
    The vector is a numpy array of floats (e.g. 5120 numbers for 14b model).
    The same text always produces the same vector.
    Texts with similar meaning produce similar vectors.
    This is the building block for similarity() and search().
    Args:
        text      (str)  — any text to convert
        use_cache (bool) — True: reuse vectors for text seen before (default)
                           False: always regenerate
    Returns:
        numpy.ndarray — vector of floats
    Example:
        vector = embed("CUDA kernel for matrix multiply")
        print(f"Vector length: {len(vector)}")    # 5120
        print(f"First 3 numbers: {vector[:3]}")   # e.g. [0.23, -0.87, 0.45]
    """
    if not text or not text.strip():
        raise ValueError("text cannot be empty")
    # Return cached result if we already computed this text
    if use_cache and text in _embedding_cache:
        return _embedding_cache[text]
    # Call Ollama to generate the vector
    response = ollama.embeddings(
        model=MODEL,
        prompt=text
    )
    vector = np.array(response['embedding'])
    # Cache it
    if use_cache:
        _embedding_cache[text] = vector
    return vector
# ─────────────────────────────────────────────────────────────────────────────
# FUNCTION 4 — similarity()
# Compare two texts by meaning. Returns a score 0.0 to 1.0
# ─────────────────────────────────────────────────────────────────────────────
def similarity(text_a, text_b):
    """
    Compare two texts and return how similar their meaning is.
    Returns a float between 0.0 and 1.0.
    Score meaning:
        1.00 = identical meaning
        0.90 = very similar
        0.70 = somewhat related
        0.50 = loosely related
        0.10 = unrelated
    Args:
        text_a (str) — first text
        text_b (str) — second text
    Returns:
        float — similarity score between 0.0 and 1.0
    Example:
        score = similarity("CUDA kernel", "GPU function")
        print(score)   # 0.91 — very similar
        score = similarity("CUDA kernel", "baking a cake")
        print(score)   # 0.12 — unrelated
    """
    vector_a = embed(text_a)
    vector_b = embed(text_b)
    # Cosine similarity:
    # dot product of both vectors divided by (length of a × length of b)
    # gives a number between -1.0 and 1.0
    # higher = more similar meaning
    dot     = np.dot(vector_a, vector_b)
    norm_a  = np.linalg.norm(vector_a)
    norm_b  = np.linalg.norm(vector_b)
    score   = dot / (norm_a * norm_b)
    return round(float(score), 4)
# ─────────────────────────────────────────────────────────────────────────────
# FUNCTION 5 — search()
# Find the most relevant item from a list by meaning.
# ─────────────────────────────────────────────────────────────────────────────
def search(query, labels, texts, top_n=3):
    """
    Find the most semantically similar items to a query from a list.
    Does NOT search for exact words — finds closest meaning.
    Args:
        query  (str)       — what you are looking for
        labels (list[str]) — names for each item e.g. ["file1.cu", "file2.cu"]
        texts  (list[str]) — content of each item  e.g. [code1, code2]
        top_n  (int)       — how many results to return (default 3)
    Returns:
        list of (score, label) tuples sorted best first
    Example:
        labels = ["kernel.cu",    "utils.h",      "main.cpp"]
        texts  = ["cuda code...", "utility code...", "main code..."]
        results = search("memory allocation", labels, texts)
        for score, label in results:
            print(f"{score:.3f} — {label}")
    """
    if len(labels) != len(texts):
        raise ValueError(
            f"labels ({len(labels)}) and texts ({len(texts)}) must be same length"
        )
    if not labels:
        raise ValueError("labels and texts cannot be empty")
    # Convert query to vector
    query_vec = embed(query)
    # Score every item
    results = []
    for label, text in zip(labels, texts):
        item_vec = embed(text)
        score = float(
            np.dot(query_vec, item_vec) /
            (np.linalg.norm(query_vec) * np.linalg.norm(item_vec))
        )
        results.append((round(score, 4), label))
    # Sort highest first
    results.sort(key=lambda x: x[0], reverse=True)
    return results[:top_n]
