#!/usr/bin/env python3
"""
Integration tests for ollama_functions.py — runs against a real local Ollama server.
Requires Ollama to be running and the configured model to be available.
Run with: python -m pytest test_ollama_functions.py -v
"""

# json is needed to manually read saved JSON files and write old-format fixtures
import json

# os is needed to delete temporary files in finally blocks
import os

# tempfile gives us throwaway files on disk without hardcoding paths
import tempfile

# unittest is the standard Python test framework — no extra install needed
import unittest

# numpy is needed to compare vectors returned by embed()
import numpy as np

# Import our module under test — no mocking, this will use the real ollama library
import ollama_functions as of


def _reset():
    """
    Reset all shared module state between tests.

    Clears the embedding cache so vectors computed in one test don't carry
    over into the next. Each test class also creates a fresh OllamaChat
    instance in setUp() so conversation history never leaks between tests.
    """
    # Wipe the module-level cache so no test inherits embeddings from a previous one
    of._embedding_cache = {}


# ─────────────────────────────────────────────────────────────────────────────
# generate() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestGenerate(unittest.TestCase):

    def setUp(self):
        # Clear the cache before every test — generate() itself doesn't cache,
        # but _reset() is called here for consistency across all test classes
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_non_empty_string(self):
        # Call the real model with a simple deterministic prompt
        result = of.generate("Say the word hello and nothing else")

        # The return value must be a str — not bytes, not None, not a dict
        self.assertIsInstance(result, str)

        # Strip whitespace before checking length — a reply of only "\n" is still empty
        self.assertGreater(len(result.strip()), 0)

    def test_stream_mode_returns_string(self):
        # Call the real model in stream mode so tokens arrive one at a time
        result = of.generate("Say the word hello and nothing else", stream=True)

        # Stream mode must accumulate all tokens and return the full reply as a string,
        # not a generator or iterator — the caller should get the same type either way
        self.assertIsInstance(result, str)

        # The accumulated result must have actual content, not just whitespace
        self.assertGreater(len(result.strip()), 0)

    # Edge cases — caught before the API is called, so no model needed ────────

    def test_empty_prompt_raises(self):
        # An empty string has no content to send — the function must catch this
        # early rather than letting Ollama return a confusing or empty response
        with self.assertRaises(ValueError):
            of.generate("")

    def test_whitespace_only_prompt_raises(self):
        # Tabs, spaces, and newlines strip down to nothing — treat as empty
        # and reject before wasting a round-trip to the model
        with self.assertRaises(ValueError):
            of.generate("   \t\n")


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.chat()
# ─────────────────────────────────────────────────────────────────────────────
class TestChat(unittest.TestCase):

    def setUp(self):
        # Clear the embedding cache — chat() doesn't embed, but good hygiene
        _reset()

        # Create a brand-new conversation for every test so no history leaks
        self.conv = of.OllamaChat()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_non_empty_string(self):
        # Send a simple message to the real model
        result = self.conv.chat("Say the word hello and nothing else")

        # The reply must be a plain string — not a dict or response object
        self.assertIsInstance(result, str)

        # Strip whitespace before checking — a newline-only reply is still empty
        self.assertGreater(len(result.strip()), 0)

    def test_history_grows_after_each_exchange(self):
        # Send two separate messages so we get two full user/assistant pairs
        self.conv.chat("What is 1 + 1?")
        self.conv.chat("And what is 2 + 2?")

        # Extract just the role field from each message to check the order
        roles = [m["role"] for m in self.conv.get_history()]

        # Expected order: system prompt, then user+assistant for each exchange
        self.assertEqual(roles, ["system", "user", "assistant", "user", "assistant"])

    def test_context_is_remembered_across_turns(self):
        # Plant a specific number in the first message
        self.conv.chat("Remember the secret number 7429. Just say OK.")

        # Ask for it back in a second message — the model can only answer correctly
        # if it still has the first message in its context window
        reply = self.conv.chat("What was the secret number I just gave you?")

        # The number must appear in the reply — if history is broken it won't
        self.assertIn("7429", reply)

    def test_stream_mode_returns_string(self):
        # Call chat() in stream mode so the model sends tokens one at a time
        result = self.conv.chat("Say the word hello and nothing else", stream=True)

        # Even in stream mode the function must return a complete string,
        # not a generator — callers should get the same type regardless of mode
        self.assertIsInstance(result, str)

        # The assembled reply must have real content, not just whitespace
        self.assertGreater(len(result.strip()), 0)

    def test_two_conversations_are_independent(self):
        # Create two separate instances — each must have its own isolated history
        conv1 = of.OllamaChat()
        conv2 = of.OllamaChat()

        # Send a message only to conv1
        conv1.chat("Remember the secret number 7429. Just say OK.")

        # conv2 never received any message — its history must still be
        # exactly 1 entry (the system prompt) and nothing from conv1
        self.assertEqual(len(conv2.get_history()), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_message_raises(self):
        # An empty string has nothing to say — reject it before the API call
        with self.assertRaises(ValueError):
            self.conv.chat("")

    def test_whitespace_only_message_raises(self):
        # Whitespace-only strips to nothing — treat the same as empty and reject
        with self.assertRaises(ValueError):
            self.conv.chat("   ")


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.clear_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestClearHistory(unittest.TestCase):

    def setUp(self):
        # Clear the embedding cache for consistency
        _reset()

        # Fresh conversation instance for every test
        self.conv = of.OllamaChat()

    def test_clears_messages_but_keeps_system_prompt(self):
        # Send a real message so the history has more than just the system prompt
        self.conv.chat("What is Python?")

        # Wipe the conversation
        self.conv.clear_history()

        # Read back the history after clearing
        history = self.conv.get_history()

        # There must be exactly one entry left — the system prompt
        self.assertEqual(len(history), 1)

        # That one entry must be the system role, not a user or assistant message
        self.assertEqual(history[0]["role"], "system")

        # The content must be our exact system prompt — not empty or changed
        self.assertEqual(history[0]["content"], of.SYSTEM_PROMPT)

    def test_clear_resets_title(self):
        # Send a message to trigger auto-title generation
        self.conv.chat("What is Python?")

        # Confirm a title was actually set before we clear
        self.assertIsNotNone(self.conv.get_title())

        # Clear the conversation
        self.conv.clear_history()

        # Title must be back to None so the next conversation gets its own title
        self.assertIsNone(self.conv.get_title())

    def test_context_is_gone_after_clear(self):
        # Plant a number in the conversation history
        self.conv.chat("Remember the secret number 9876. Just say OK.")

        # Wipe the history — the model should no longer know about 9876
        self.conv.clear_history()

        # Ask for the number — the model has no context so it can't know it
        reply = self.conv.chat("What was the secret number I just gave you?")

        # 9876 must not appear — if it does, clear_history() didn't work
        self.assertNotIn("9876", reply)

    def test_clear_on_fresh_history_is_idempotent(self):
        # Clear a conversation that has never had any messages
        self.conv.clear_history()

        # Clear again — calling it twice must not break anything
        self.conv.clear_history()

        # Should still have exactly one entry: the system prompt
        self.assertEqual(len(self.conv.get_history()), 1)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.get_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestGetHistory(unittest.TestCase):

    def setUp(self):
        # Clear cache for consistency
        _reset()

        # Fresh conversation instance
        self.conv = of.OllamaChat()

    def test_returns_copy_not_reference(self):
        # Get the history list — this should be a copy, not the real internal list
        history = self.conv.get_history()

        # Append a fake message to the returned list
        history.append({"role": "user", "content": "injected"})

        # Call get_history() again — if it returned a reference, the injected
        # message would now be inside the conversation, which would be a bug
        self.assertEqual(len(self.conv.get_history()), 1)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.save_history() / OllamaChat.load_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestSaveLoadHistory(unittest.TestCase):

    def setUp(self):
        # Clear cache for consistency
        _reset()

        # Fresh conversation instance
        self.conv = of.OllamaChat()

    def test_save_then_load_roundtrip(self):
        # Have a real exchange so the history contains actual model output
        self.conv.chat("What is a CUDA warp?")

        # Snapshot the current history and title before saving
        original_history = self.conv.get_history()
        original_title   = self.conv.get_title()

        # Create a temporary file — delete=False so we control when it's removed
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            # Write the conversation to disk
            self.conv.save_history(path)

            # Wipe the conversation completely so load() has to do real work
            self.conv.clear_history()

            # Confirm the wipe worked — title must be gone before we reload
            self.assertIsNone(self.conv.get_title())

            # Restore from disk
            self.conv.load_history(path)

            # History must match the snapshot we took before saving
            self.assertEqual(self.conv.get_history(), original_history)

            # Title must also be restored to exactly what it was
            self.assertEqual(self.conv.get_title(), original_title)
        finally:
            # Always clean up the temp file, even if the test fails
            os.unlink(path)

    def test_load_old_format_list(self):
        # Build a plain list — the format used before titles were added
        old_data = [
            {"role": "system", "content": "sys"},
            {"role": "user",   "content": "hi"},
        ]

        # Write it to a temp file as raw JSON
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(old_data, tmp)
            path = tmp.name
        try:
            # Load the old-format file — must not crash
            self.conv.load_history(path)

            # Both messages must be present
            self.assertEqual(len(self.conv.get_history()), 2)

            # Old format has no title field — must default to None
            self.assertIsNone(self.conv.get_title())
        finally:
            os.unlink(path)

    def test_save_persists_title(self):
        # Set a title manually so we know exactly what to expect in the file
        self.conv.set_title("My CUDA Session")

        # Create a temp file for saving
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            # Write to disk
            self.conv.save_history(path)

            # Read the raw JSON back to inspect the file structure directly
            with open(path) as f:
                data = json.load(f)

            # The title we set must be in the file
            self.assertEqual(data["title"], "My CUDA Session")

            # The messages list must also be present — without it load() would fail
            self.assertIn("messages", data)
        finally:
            os.unlink(path)

    def test_load_missing_file_raises(self):
        # Use a path that cannot possibly exist
        with self.assertRaises(FileNotFoundError):
            self.conv.load_history("/nonexistent/path/session.json")

    def test_load_corrupted_json_raises(self):
        # Write invalid JSON to a temp file to simulate a damaged file
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            tmp.write("not valid json {{{{")
            path = tmp.name
        try:
            # Loading a broken file must raise a JSON error, not silently succeed
            with self.assertRaises(json.JSONDecodeError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_save_returns_filepath(self):
        # Create a temp file to save into
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            # Save and capture the return value
            returned = self.conv.save_history(path)

            # The returned path must match what we passed in — callers rely on
            # this especially when using the auto-generated default path
            self.assertEqual(returned, path)
        finally:
            os.unlink(path)

    def test_load_into_separate_instance(self):
        # Have a real exchange on the original instance
        self.conv.chat("What is shared memory in CUDA?")

        # Create a temp file for saving
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            # Save the original conversation to disk
            self.conv.save_history(path)

            # Create a completely separate instance — no shared state with self.conv
            new_conv = of.OllamaChat()

            # Load the saved file into the new instance
            new_conv.load_history(path)

            # The new instance must have the same title as the original
            self.assertEqual(new_conv.get_title(), self.conv.get_title())

            # The new instance must have the same full history as the original
            self.assertEqual(new_conv.get_history(), self.conv.get_history())
        finally:
            os.unlink(path)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.get_title() / set_title() / auto-title
# ─────────────────────────────────────────────────────────────────────────────
class TestTitle(unittest.TestCase):

    def setUp(self):
        # Clear cache for consistency
        _reset()

        # Fresh conversation instance
        self.conv = of.OllamaChat()

    def test_title_none_before_any_chat(self):
        # No messages have been sent — title must not exist yet
        self.assertIsNone(self.conv.get_title())

    def test_title_auto_generated_after_first_chat(self):
        # Send a real first message — this triggers auto-title generation internally
        self.conv.chat("Explain CUDA warp divergence")

        # Read the title back
        title = self.conv.get_title()

        # Must be a string — not None, not a number
        self.assertIsInstance(title, str)

        # Must have actual content — an empty or whitespace title is not useful
        self.assertGreater(len(title.strip()), 0)

    def test_title_not_regenerated_on_second_message(self):
        # Send the first message — title is generated from this one
        self.conv.chat("Explain CUDA warp divergence")

        # Capture the title set by the first message
        title_after_first = self.conv.get_title()

        # Send a second message — this must NOT trigger a new title
        self.conv.chat("Now show me a code example")

        # Capture the title again
        title_after_second = self.conv.get_title()

        # Both titles must be identical — the first one must be locked in
        self.assertEqual(title_after_first, title_after_second)

    def test_set_title_overwrites_auto_title(self):
        # Send a message so an auto-title is generated
        self.conv.chat("Explain CUDA warp divergence")

        # Override it with a manual title
        self.conv.set_title("My Custom Title")

        # The manual title must now be returned — not the auto-generated one
        self.assertEqual(self.conv.get_title(), "My Custom Title")

    def test_set_title_strips_whitespace(self):
        # Set a title with leading and trailing spaces
        self.conv.set_title("  padded  ")

        # The stored title must have the whitespace removed
        self.assertEqual(self.conv.get_title(), "padded")

    def test_set_title_before_any_chat(self):
        # Set a title before sending any messages
        self.conv.set_title("Pre-set Title")

        # Must be stored and returned immediately — no chat() required
        self.assertEqual(self.conv.get_title(), "Pre-set Title")

    def test_set_title_empty_raises(self):
        # An empty string is not a valid title — must be rejected
        with self.assertRaises(ValueError):
            self.conv.set_title("")

    def test_set_title_whitespace_raises(self):
        # Whitespace-only strips to nothing — treat as empty and reject
        with self.assertRaises(ValueError):
            self.conv.set_title("   ")

    def test_two_instances_have_independent_titles(self):
        # Create two separate instances
        conv1 = of.OllamaChat()
        conv2 = of.OllamaChat()

        # Set different titles on each
        conv1.set_title("Title One")
        conv2.set_title("Title Two")

        # Each instance must return its own title — not the other's
        self.assertEqual(conv1.get_title(), "Title One")
        self.assertEqual(conv2.get_title(), "Title Two")


# ─────────────────────────────────────────────────────────────────────────────
# embed() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestEmbed(unittest.TestCase):

    def setUp(self):
        # Clear the cache so every test starts with no pre-computed vectors
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_numpy_array(self):
        # Call the real embedding API
        result = of.embed("CUDA kernel")

        # Must be a numpy array — similarity() and search() need to do math on it
        self.assertIsInstance(result, np.ndarray)

    def test_vector_is_not_empty(self):
        # Call the real embedding API
        result = of.embed("CUDA kernel")

        # A zero-length array would make all downstream math crash or produce nonsense
        self.assertGreater(len(result), 0)

    def test_same_text_gives_same_vector(self):
        # Embed the same text twice, bypassing cache both times to force two API calls
        vec1 = of.embed("shared memory", use_cache=False)
        vec2 = of.embed("shared memory", use_cache=False)

        # Embeddings are deterministic — the two vectors must be numerically identical
        np.testing.assert_array_almost_equal(vec1, vec2)

    def test_caching_avoids_second_api_call(self):
        # Embed text for the first time — this must hit the API and store in cache
        of.embed("same text")

        # The cache must now contain an entry for this text
        self.assertIn("same text", of._embedding_cache)

        # Take a snapshot of the cached vector
        cached = of._embedding_cache["same text"].copy()

        # Embed the same text again with cache enabled
        of.embed("same text")

        # The cached vector must be unchanged — proving the second call used the cache
        np.testing.assert_array_equal(of._embedding_cache["same text"], cached)

    def test_use_cache_false_bypasses_cache(self):
        # Embed with cache disabled — the result must NOT be stored
        of.embed("text", use_cache=False)

        # Nothing should have been written to the cache dict
        self.assertNotIn("text", of._embedding_cache)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_text_raises(self):
        # Empty string has no semantic content — reject before calling the API
        with self.assertRaises(ValueError):
            of.embed("")

    def test_whitespace_only_raises(self):
        # Whitespace strips to nothing — treat the same as empty and reject
        with self.assertRaises(ValueError):
            of.embed("   ")


# ─────────────────────────────────────────────────────────────────────────────
# similarity() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSimilarity(unittest.TestCase):

    def setUp(self):
        # Clear the cache so each test embeds fresh — no cross-test interference
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_float(self):
        # Compare two real texts through the real model
        score = of.similarity("CUDA kernel", "GPU function")

        # Must be a plain Python float — not a numpy scalar, not None
        self.assertIsInstance(score, float)

    def test_score_in_valid_range(self):
        # Compare two real texts
        score = of.similarity("CUDA kernel", "GPU function")

        # Cosine similarity is mathematically bounded to [-1.0, 1.0]
        # Anything outside that range means something went wrong in the math
        self.assertGreaterEqual(score, -1.0)
        self.assertLessEqual(score, 1.0)

    def test_identical_text_scores_near_one(self):
        # Compare a string to itself — the vectors will be identical
        score = of.similarity("warp divergence", "warp divergence")

        # Cosine similarity of a vector with itself is exactly 1.0
        # We use 0.99 as the threshold to allow for any floating-point rounding
        self.assertGreater(score, 0.99)

    def test_related_texts_score_higher_than_unrelated(self):
        # Compare two texts that share domain meaning
        related = of.similarity("CUDA kernel", "GPU function")

        # Compare two texts that have nothing in common
        unrelated = of.similarity("CUDA kernel", "chocolate cake recipe")

        # The related pair must score higher — this is the core value of embeddings
        self.assertGreater(related, unrelated)

    def test_score_rounded_to_4_decimal_places(self):
        # Compare two real texts
        score = of.similarity("hello", "world")

        # Scores are rounded to 4 decimal places to keep them clean and stable
        # We verify by checking the score equals itself rounded — if it had more
        # decimal places this assertion would fail
        self.assertEqual(score, round(score, 4))


# ─────────────────────────────────────────────────────────────────────────────
# search() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSearch(unittest.TestCase):

    def setUp(self):
        # Clear the cache so embeddings don't carry over from previous tests
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_list_of_tuples(self):
        # Run a real search with two clearly different topics
        results = of.search("memory", ["topic_a", "topic_b"], ["CUDA memory", "Python loops"])

        # Return value must be a list
        self.assertIsInstance(results, list)

        # Unpack the first result to confirm it's a (score, label) tuple
        score, label = results[0]

        # Score must be a float so it can be compared numerically
        self.assertIsInstance(score, float)

        # Label must be a string matching one of the labels we passed in
        self.assertIsInstance(label, str)

    def test_returns_top_n_results(self):
        # Define three items but ask for only two results
        labels = ["a", "b", "c"]
        texts  = ["CUDA memory", "Python GIL", "warp divergence"]

        # Run search with top_n=2
        results = of.search("memory management", labels, texts, top_n=2)

        # Must return exactly 2 results — not 3, not 1
        self.assertEqual(len(results), 2)

    def test_results_sorted_best_first(self):
        # Use one clearly on-topic item and one clearly off-topic item
        labels = ["memory_topic", "unrelated_topic"]
        texts  = ["GPU memory allocation and CUDA malloc", "French cooking techniques"]

        # Search for something related to the first item
        results = of.search("CUDA memory allocation", labels, texts, top_n=2)

        # The first result must have a score greater than or equal to the second
        self.assertGreaterEqual(results[0][0], results[1][0])

    def test_most_relevant_item_ranked_first(self):
        # Use two items with completely different domains so there's no ambiguity
        labels = ["cuda_doc",   "cooking_doc"]
        texts  = [
            "CUDA shared memory allows threads in a warp to share data",
            "To make pasta, boil water and add salt",
        ]

        # Search for something clearly about CUDA
        results = of.search("GPU shared memory between threads", labels, texts, top_n=2)

        # The CUDA document must be ranked first
        self.assertEqual(results[0][1], "cuda_doc")

    def test_top_n_larger_than_list_returns_all(self):
        # Pass a list with only one item but ask for 100 results
        results = of.search("memory", ["only_one"], ["CUDA memory"], top_n=100)

        # Can't return more than what exists — must return exactly 1
        self.assertEqual(len(results), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_mismatched_labels_texts_raises(self):
        # Two labels but only one text — the pairing is broken
        with self.assertRaises(ValueError):
            of.search("q", ["a", "b"], ["only_one"])

    def test_empty_labels_raises(self):
        # Nothing to search over — must be caught before any embedding call
        with self.assertRaises(ValueError):
            of.search("q", [], [])

    def test_empty_query_raises(self):
        # An empty query has no meaning to search for — reject before embedding
        with self.assertRaises(ValueError):
            of.search("", ["a"], ["some text"])

    def test_whitespace_query_raises(self):
        # Whitespace-only strips to nothing — treat as empty and reject
        with self.assertRaises(ValueError):
            of.search("   ", ["a"], ["some text"])

    def test_top_n_zero_raises(self):
        # Asking for zero results makes no sense — reject it
        with self.assertRaises(ValueError):
            of.search("query", ["a"], ["text"], top_n=0)

    def test_top_n_negative_raises(self):
        # A negative top_n is nonsensical — reject it
        with self.assertRaises(ValueError):
            of.search("query", ["a"], ["text"], top_n=-1)


# ─────────────────────────────────────────────────────────────────────────────
# Error handling — invalid parameters caught before any API call
# These tests never touch the network so they pass whether Ollama is up or not
# ─────────────────────────────────────────────────────────────────────────────
class TestGenerateErrors(unittest.TestCase):

    def setUp(self):
        # Clear cache for consistency even though generate() doesn't use it
        _reset()

    def test_temperature_below_zero_raises(self):
        # Temperature below 0.0 is outside the valid range — reject early
        with self.assertRaises(ValueError):
            of.generate("hello", temperature=-0.1)

    def test_temperature_above_two_raises(self):
        # Temperature above 2.0 is outside the valid range — reject early
        with self.assertRaises(ValueError):
            of.generate("hello", temperature=2.1)

    def test_max_tokens_zero_raises(self):
        # Zero tokens means no output — there is nothing useful to return
        with self.assertRaises(ValueError):
            of.generate("hello", max_tokens=0)

    def test_max_tokens_negative_raises(self):
        # A negative token limit is nonsensical — reject it
        with self.assertRaises(ValueError):
            of.generate("hello", max_tokens=-1)


class TestChatErrors(unittest.TestCase):

    def setUp(self):
        # Clear cache and create a fresh conversation for every test
        _reset()
        self.conv = of.OllamaChat()

    def test_temperature_below_zero_raises(self):
        # Temperature below 0.0 is invalid — reject before touching history
        with self.assertRaises(ValueError):
            self.conv.chat("hello", temperature=-0.1)

    def test_temperature_above_two_raises(self):
        # Temperature above 2.0 is invalid — reject before touching history
        with self.assertRaises(ValueError):
            self.conv.chat("hello", temperature=2.1)

    def test_max_tokens_zero_raises(self):
        # Zero max_tokens means no output — reject before touching history
        with self.assertRaises(ValueError):
            self.conv.chat("hello", max_tokens=0)

    def test_max_tokens_negative_raises(self):
        # A negative token limit is nonsensical — reject before touching history
        with self.assertRaises(ValueError):
            self.conv.chat("hello", max_tokens=-1)

    def test_history_not_modified_on_invalid_temperature(self):
        # When validation fails, the user message must NOT be appended to history.
        # If it were, the next successful chat() would send a dangling user message
        # with no corresponding assistant reply, corrupting the conversation.
        history_before = len(self.conv.get_history())

        # Attempt a chat with an invalid temperature — must raise before modifying history
        with self.assertRaises(ValueError):
            self.conv.chat("hello", temperature=99.0)

        # History length must be identical to what it was before the failed call
        self.assertEqual(len(self.conv.get_history()), history_before)


class TestOllamaChatInitErrors(unittest.TestCase):

    def test_empty_model_raises(self):
        # An empty model name cannot be sent to Ollama — reject at construction time
        with self.assertRaises(ValueError):
            of.OllamaChat(model="")

    def test_whitespace_model_raises(self):
        # A whitespace-only model name is the same as empty — reject it
        with self.assertRaises(ValueError):
            of.OllamaChat(model="   ")

    def test_empty_system_prompt_raises(self):
        # An empty system prompt gives the model no instructions — reject it
        with self.assertRaises(ValueError):
            of.OllamaChat(system_prompt="")

    def test_whitespace_system_prompt_raises(self):
        # A whitespace-only system prompt strips to nothing — reject it
        with self.assertRaises(ValueError):
            of.OllamaChat(system_prompt="   ")


class TestLoadHistoryErrors(unittest.TestCase):

    def setUp(self):
        # Clear cache and create a fresh conversation for every test
        _reset()
        self.conv = of.OllamaChat()

    def test_load_missing_messages_key_raises(self):
        # A JSON object without a "messages" key is not a valid history file
        bad_data = {"title": "something", "wrong_key": []}

        # Write the bad data to a temp file
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(bad_data, tmp)
            path = tmp.name
        try:
            # Must raise ValueError — not KeyError or AttributeError
            with self.assertRaises(ValueError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_load_wrong_root_type_raises(self):
        # A JSON file whose root is a number is not a valid history format
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(42, tmp)
            path = tmp.name
        try:
            # Must raise ValueError with a clear message about the type
            with self.assertRaises(ValueError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_load_message_missing_role_raises(self):
        # A message without a "role" field is not a valid history entry
        bad_data = {"messages": [{"content": "hello"}]}

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(bad_data, tmp)
            path = tmp.name
        try:
            # Must raise ValueError — not KeyError
            with self.assertRaises(ValueError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_load_message_missing_content_raises(self):
        # A message without a "content" field is not a valid history entry
        bad_data = {"messages": [{"role": "user"}]}

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(bad_data, tmp)
            path = tmp.name
        try:
            # Must raise ValueError — not KeyError
            with self.assertRaises(ValueError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_load_messages_not_a_list_raises(self):
        # "messages" must be a list — a dict or string is not valid
        bad_data = {"messages": "not a list"}

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(bad_data, tmp)
            path = tmp.name
        try:
            # Must raise ValueError
            with self.assertRaises(ValueError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)


class TestSaveHistoryErrors(unittest.TestCase):

    def setUp(self):
        # Clear cache and create a fresh conversation for every test
        _reset()
        self.conv = of.OllamaChat()

    def test_save_to_unwritable_path_raises(self):
        # A path inside a directory that doesn't exist and can't be created
        # should raise OSError — not silently fail or create garbage files
        with self.assertRaises(OSError):
            self.conv.save_history("/root/nonexistent_dir/session.json")


if __name__ == "__main__":
    unittest.main()
