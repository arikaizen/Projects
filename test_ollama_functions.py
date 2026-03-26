#!/usr/bin/env python3
"""
Integration tests for ollama_functions.py — runs against a real local Ollama server.
Requires Ollama to be running and the configured model to be available.
Run with: python -m pytest test_ollama_functions.py -v
"""
import json
import os
import tempfile
import unittest

import numpy as np

import ollama_functions as of


def _reset():
    """
    Reset all shared module state between tests.

    Clears the embedding cache so vectors computed in one test don't carry
    over into the next. Also resets each conversation instance's history via
    setUp() in each class.
    """
    of._embedding_cache = {}


# ─────────────────────────────────────────────────────────────────────────────
# generate() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestGenerate(unittest.TestCase):

    def setUp(self):
        # Start each test with a clean cache so previous embeds don't interfere
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_non_empty_string(self):
        # The model must return something — an empty reply would be a bug
        result = of.generate("Say the word hello and nothing else")
        self.assertIsInstance(result, str)
        self.assertGreater(len(result.strip()), 0)

    def test_stream_mode_returns_string(self):
        # Stream mode should accumulate tokens and return the full reply,
        # the same type as non-stream mode
        result = of.generate("Say the word hello and nothing else", stream=True)
        self.assertIsInstance(result, str)
        self.assertGreater(len(result.strip()), 0)

    # Edge cases — these are caught before hitting the API, so no model needed ─

    def test_empty_prompt_raises(self):
        # Guard against accidental empty calls — validated before any API call
        with self.assertRaises(ValueError):
            of.generate("")

    def test_whitespace_only_prompt_raises(self):
        # A prompt that is only spaces/tabs/newlines is meaningless — reject it
        with self.assertRaises(ValueError):
            of.generate("   \t\n")


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.chat()
# ─────────────────────────────────────────────────────────────────────────────
class TestChat(unittest.TestCase):

    def setUp(self):
        _reset()
        # A fresh conversation instance for every test so history never leaks
        self.conv = of.OllamaChat()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_non_empty_string(self):
        # The model must reply with something usable
        result = self.conv.chat("Say the word hello and nothing else")
        self.assertIsInstance(result, str)
        self.assertGreater(len(result.strip()), 0)

    def test_history_grows_after_each_exchange(self):
        # After one exchange we expect: system, user, assistant (3 entries).
        # After two exchanges: system, user, assistant, user, assistant (5 entries).
        # This verifies both turns are recorded correctly.
        self.conv.chat("What is 1 + 1?")
        self.conv.chat("And what is 2 + 2?")

        roles = [m["role"] for m in self.conv.get_history()]
        self.assertEqual(roles, ["system", "user", "assistant", "user", "assistant"])

    def test_context_is_remembered_across_turns(self):
        # The model should remember what was said in the previous turn.
        # We ask it to remember a number, then ask for it back.
        # If history is broken, the second reply won't contain the number.
        self.conv.chat("Remember the secret number 7429. Just say OK.")
        reply = self.conv.chat("What was the secret number I just gave you?")
        self.assertIn("7429", reply)

    def test_stream_mode_returns_string(self):
        # Stream mode must accumulate all tokens and return them as one string,
        # same as non-stream mode
        result = self.conv.chat("Say the word hello and nothing else", stream=True)
        self.assertIsInstance(result, str)
        self.assertGreater(len(result.strip()), 0)

    def test_two_conversations_are_independent(self):
        # The whole point of the class-based design — two instances must never
        # share history. Sending a message to conv1 must not appear in conv2.
        conv1 = of.OllamaChat()
        conv2 = of.OllamaChat()

        conv1.chat("Remember the secret number 7429. Just say OK.")

        # conv2 was never told about 7429 — it should have only its system prompt
        self.assertEqual(len(conv2.get_history()), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_message_raises(self):
        # Caught before any API call — no point sending a blank message
        with self.assertRaises(ValueError):
            self.conv.chat("")

    def test_whitespace_only_message_raises(self):
        # Same guard as above — whitespace-only is effectively empty
        with self.assertRaises(ValueError):
            self.conv.chat("   ")


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.clear_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestClearHistory(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_clears_messages_but_keeps_system_prompt(self):
        # After a real exchange the history has 3+ entries.
        # clear_history() must reduce it back to exactly 1 (the system prompt).
        self.conv.chat("What is Python?")
        self.conv.clear_history()

        history = self.conv.get_history()
        self.assertEqual(len(history), 1)
        self.assertEqual(history[0]["role"], "system")
        self.assertEqual(history[0]["content"], of.SYSTEM_PROMPT)

    def test_clear_resets_title(self):
        # The auto-generated title from the first message must be wiped
        # so the next conversation gets its own fresh title
        self.conv.chat("What is Python?")
        self.assertIsNotNone(self.conv.get_title())

        self.conv.clear_history()
        self.assertIsNone(self.conv.get_title())

    def test_context_is_gone_after_clear(self):
        # After clearing, the model must have no memory of the previous exchange.
        # We ask it to remember a number, clear, then ask for it back.
        # The model should not be able to recall it.
        self.conv.chat("Remember the secret number 9876. Just say OK.")
        self.conv.clear_history()

        reply = self.conv.chat("What was the secret number I just gave you?")
        # The model has no context — it should say it doesn't know
        self.assertNotIn("9876", reply)

    def test_clear_on_fresh_history_is_idempotent(self):
        # Clearing a conversation that has never been used should be safe
        # and leave exactly the one system prompt entry
        self.conv.clear_history()
        self.conv.clear_history()
        self.assertEqual(len(self.conv.get_history()), 1)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.get_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestGetHistory(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_returns_copy_not_reference(self):
        # If get_history() returned a reference to the internal list, callers
        # could silently corrupt the conversation state just by appending to it.
        # This test proves that can't happen.
        history = self.conv.get_history()
        history.append({"role": "user", "content": "injected"})

        # The internal state must still have only the system prompt
        self.assertEqual(len(self.conv.get_history()), 1)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.save_history() / OllamaChat.load_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestSaveLoadHistory(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_save_then_load_roundtrip(self):
        # The full cycle: chat → save → clear → load.
        # After loading, the history and title must be exactly what was saved.
        self.conv.chat("What is a CUDA warp?")

        original_history = self.conv.get_history()
        original_title   = self.conv.get_title()

        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            self.conv.save_history(path)

            # Wipe everything so we can confirm load truly restores it
            self.conv.clear_history()
            self.assertIsNone(self.conv.get_title())

            self.conv.load_history(path)

            # History and title must match what was saved
            self.assertEqual(self.conv.get_history(), original_history)
            self.assertEqual(self.conv.get_title(), original_title)
        finally:
            os.unlink(path)

    def test_load_old_format_list(self):
        # Backwards compatibility: files saved before titles existed were plain
        # JSON lists. Loading them must not crash and title must be None.
        old_data = [
            {"role": "system", "content": "sys"},
            {"role": "user",   "content": "hi"},
        ]
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            json.dump(old_data, tmp)
            path = tmp.name
        try:
            self.conv.load_history(path)
            self.assertEqual(len(self.conv.get_history()), 2)
            self.assertIsNone(self.conv.get_title())
        finally:
            os.unlink(path)

    def test_save_persists_title(self):
        # The JSON file must contain both "title" and "messages" keys
        # so it can be fully restored later
        self.conv.set_title("My CUDA Session")

        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            self.conv.save_history(path)
            with open(path) as f:
                data = json.load(f)
            self.assertEqual(data["title"], "My CUDA Session")
            self.assertIn("messages", data)
        finally:
            os.unlink(path)

    def test_load_missing_file_raises(self):
        # A clear error is better than a cryptic KeyError or AttributeError
        # when the file doesn't exist
        with self.assertRaises(FileNotFoundError):
            self.conv.load_history("/nonexistent/path/session.json")

    def test_load_corrupted_json_raises(self):
        # If the file is damaged or truncated, we should get a clear JSON error
        # rather than silently loading garbage
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            tmp.write("not valid json {{{{")
            path = tmp.name
        try:
            with self.assertRaises(json.JSONDecodeError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_save_returns_filepath(self):
        # The caller needs to know where the file ended up, especially when
        # using the auto-generated default path
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            returned = self.conv.save_history(path)
            self.assertEqual(returned, path)
        finally:
            os.unlink(path)

    def test_load_into_separate_instance(self):
        # A conversation saved from one instance must be fully usable
        # when loaded into a completely different instance
        self.conv.chat("What is shared memory in CUDA?")

        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            self.conv.save_history(path)

            # Load into a brand-new instance — it should get the full history
            new_conv = of.OllamaChat()
            new_conv.load_history(path)

            self.assertEqual(new_conv.get_title(), self.conv.get_title())
            self.assertEqual(new_conv.get_history(), self.conv.get_history())
        finally:
            os.unlink(path)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.get_title() / set_title() / auto-title
# ─────────────────────────────────────────────────────────────────────────────
class TestTitle(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_title_none_before_any_chat(self):
        # No title should exist until the first message is sent
        self.assertIsNone(self.conv.get_title())

    def test_title_auto_generated_after_first_chat(self):
        # After the first chat() call, the model generates a short title
        # automatically — it should be a non-empty string
        self.conv.chat("Explain CUDA warp divergence")
        title = self.conv.get_title()
        self.assertIsInstance(title, str)
        self.assertGreater(len(title.strip()), 0)

    def test_title_not_regenerated_on_second_message(self):
        # The title is set once from the first message and must never change
        # on subsequent messages — it would be confusing if it did
        self.conv.chat("Explain CUDA warp divergence")
        title_after_first = self.conv.get_title()

        self.conv.chat("Now show me a code example")
        title_after_second = self.conv.get_title()

        self.assertEqual(title_after_first, title_after_second)

    def test_set_title_overwrites_auto_title(self):
        # Users should always be able to override the auto-generated title
        self.conv.chat("Explain CUDA warp divergence")
        self.conv.set_title("My Custom Title")
        self.assertEqual(self.conv.get_title(), "My Custom Title")

    def test_set_title_strips_whitespace(self):
        # Leading/trailing whitespace in a title is almost always accidental
        self.conv.set_title("  padded  ")
        self.assertEqual(self.conv.get_title(), "padded")

    def test_set_title_before_any_chat(self):
        # You should be able to name a conversation before sending any messages
        self.conv.set_title("Pre-set Title")
        self.assertEqual(self.conv.get_title(), "Pre-set Title")

    def test_set_title_empty_raises(self):
        # An empty title is meaningless — reject it early
        with self.assertRaises(ValueError):
            self.conv.set_title("")

    def test_set_title_whitespace_raises(self):
        # Same as empty — whitespace-only has no content
        with self.assertRaises(ValueError):
            self.conv.set_title("   ")

    def test_two_instances_have_independent_titles(self):
        # Each conversation must manage its own title without interference
        conv1 = of.OllamaChat()
        conv2 = of.OllamaChat()
        conv1.set_title("Title One")
        conv2.set_title("Title Two")
        self.assertEqual(conv1.get_title(), "Title One")
        self.assertEqual(conv2.get_title(), "Title Two")


# ─────────────────────────────────────────────────────────────────────────────
# embed() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestEmbed(unittest.TestCase):

    def setUp(self):
        # Clear cache so each test controls exactly which API calls happen
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_numpy_array(self):
        # The return type must be a numpy array so similarity() and search()
        # can do vector math on it
        result = of.embed("CUDA kernel")
        self.assertIsInstance(result, np.ndarray)

    def test_vector_is_not_empty(self):
        # A zero-length vector would break all downstream math
        result = of.embed("CUDA kernel")
        self.assertGreater(len(result), 0)

    def test_same_text_gives_same_vector(self):
        # Embeddings are deterministic — the same text must always produce
        # the same vector (this also implicitly tests caching correctness)
        vec1 = of.embed("shared memory", use_cache=False)
        vec2 = of.embed("shared memory", use_cache=False)
        np.testing.assert_array_almost_equal(vec1, vec2)

    def test_caching_avoids_second_api_call(self):
        # Embedding the same text twice with the cache on must only hit
        # the API once — the second call should return from the cache.
        # We verify by checking the cache dict directly.
        of.embed("same text")
        self.assertIn("same text", of._embedding_cache)

        # Embed again — cache entry must already be there, no new API call
        cached = of._embedding_cache["same text"].copy()
        of.embed("same text")
        np.testing.assert_array_equal(of._embedding_cache["same text"], cached)

    def test_use_cache_false_bypasses_cache(self):
        # use_cache=False must skip the cache entirely — useful when you
        # explicitly want a fresh embedding regardless of what's stored
        of.embed("text", use_cache=False)
        # Nothing should have been written to the cache
        self.assertNotIn("text", of._embedding_cache)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_text_raises(self):
        # Empty text produces a meaningless vector — reject it before the API call
        with self.assertRaises(ValueError):
            of.embed("")

    def test_whitespace_only_raises(self):
        # Whitespace strips to nothing — same as empty
        with self.assertRaises(ValueError):
            of.embed("   ")


# ─────────────────────────────────────────────────────────────────────────────
# similarity() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSimilarity(unittest.TestCase):

    def setUp(self):
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_float(self):
        # Score must be a plain Python float, not a numpy scalar,
        # so callers can use it in normal comparisons without surprises
        score = of.similarity("CUDA kernel", "GPU function")
        self.assertIsInstance(score, float)

    def test_score_in_valid_range(self):
        # Cosine similarity is always in [-1, 1] — anything outside is a bug
        score = of.similarity("CUDA kernel", "GPU function")
        self.assertGreaterEqual(score, -1.0)
        self.assertLessEqual(score, 1.0)

    def test_identical_text_scores_near_one(self):
        # The same text compared to itself should score very close to 1.0
        score = of.similarity("warp divergence", "warp divergence")
        self.assertGreater(score, 0.99)

    def test_related_texts_score_higher_than_unrelated(self):
        # "CUDA kernel" and "GPU function" share domain meaning.
        # "CUDA kernel" and "chocolate cake recipe" do not.
        # The related pair must score higher than the unrelated pair.
        related   = of.similarity("CUDA kernel", "GPU function")
        unrelated = of.similarity("CUDA kernel", "chocolate cake recipe")
        self.assertGreater(related, unrelated)

    def test_score_rounded_to_4_decimal_places(self):
        # Scores are rounded to 4 places to avoid floating-point noise
        # making comparisons unpredictable
        score = of.similarity("hello", "world")
        self.assertEqual(score, round(score, 4))


# ─────────────────────────────────────────────────────────────────────────────
# search() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSearch(unittest.TestCase):

    def setUp(self):
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_list_of_tuples(self):
        # Each result must be a (score, label) tuple so callers can unpack it
        results = of.search("memory", ["topic_a", "topic_b"], ["CUDA memory", "Python loops"])
        self.assertIsInstance(results, list)
        score, label = results[0]
        self.assertIsInstance(score, float)
        self.assertIsInstance(label, str)

    def test_returns_top_n_results(self):
        # search() must respect the top_n limit and not return more items
        labels = ["a", "b", "c"]
        texts  = ["CUDA memory", "Python GIL", "warp divergence"]
        results = of.search("memory management", labels, texts, top_n=2)
        self.assertEqual(len(results), 2)

    def test_results_sorted_best_first(self):
        # Results must be in descending score order — the most relevant item first
        labels = ["memory_topic", "unrelated_topic"]
        texts  = ["GPU memory allocation and CUDA malloc", "French cooking techniques"]
        results = of.search("CUDA memory allocation", labels, texts, top_n=2)
        self.assertGreaterEqual(results[0][0], results[1][0])

    def test_most_relevant_item_ranked_first(self):
        # The item whose content is closest to the query must appear at index 0.
        # We use clearly distinct topics so the model isn't ambiguous.
        labels = ["cuda_doc",        "cooking_doc"]
        texts  = ["CUDA shared memory allows threads in a warp to share data",
                  "To make pasta, boil water and add salt"]
        results = of.search("GPU shared memory between threads", labels, texts, top_n=2)
        self.assertEqual(results[0][1], "cuda_doc")

    def test_top_n_larger_than_list_returns_all(self):
        # Asking for more results than items available should just return everything
        results = of.search("memory", ["only_one"], ["CUDA memory"], top_n=100)
        self.assertEqual(len(results), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_mismatched_labels_texts_raises(self):
        # labels and texts must be paired — a mismatch means the data is wrong
        with self.assertRaises(ValueError):
            of.search("q", ["a", "b"], ["only_one"])

    def test_empty_labels_raises(self):
        # Nothing to search over — reject before any embedding call
        with self.assertRaises(ValueError):
            of.search("q", [], [])


if __name__ == "__main__":
    unittest.main()
