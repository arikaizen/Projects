#!/usr/bin/env python3
"""
Tests for ollama_functions.py
Run with: python -m pytest test_ollama_functions.py -v
"""
import json
import os
import tempfile
import unittest
from unittest.mock import MagicMock, patch

import numpy as np

# ── Patch ollama before importing our module so no real Ollama server is needed ─
ollama_mock = MagicMock()
import sys
sys.modules.setdefault("ollama", ollama_mock)

import ollama_functions as of


def _reset():
    """
    Reset all shared state between tests.

    Clears the module-level embedding cache and resets the ollama mock so
    each test starts from a clean slate.
    """
    of._embedding_cache = {}
    ollama_mock.reset_mock()
    # reset_mock() does NOT clear side_effect by default — do it explicitly
    ollama_mock.generate.side_effect = None
    ollama_mock.chat.side_effect = None
    ollama_mock.embeddings.side_effect = None


# ─────────────────────────────────────────────────────────────────────────────
# generate() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestGenerate(unittest.TestCase):

    def setUp(self):
        _reset()

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_response_string(self):
        ollama_mock.generate.return_value = {"response": "hello"}
        result = of.generate("write a kernel")
        self.assertEqual(result, "hello")

    def test_passes_prompt_and_options(self):
        ollama_mock.generate.return_value = {"response": "ok"}
        of.generate("my prompt", temperature=0.2, max_tokens=512)
        ollama_mock.generate.assert_called_once_with(
            model=of.MODEL,
            prompt="my prompt",
            stream=False,
            options={"temperature": 0.2, "num_predict": 512},
        )

    def test_stream_mode_prints_and_returns_full_text(self):
        chunks = [{"response": "tok1"}, {"response": " tok2"}]
        ollama_mock.generate.return_value = iter(chunks)
        with patch("builtins.print") as mock_print:
            result = of.generate("prompt", stream=True)
        self.assertEqual(result, "tok1 tok2")
        mock_print.assert_any_call("tok1", end="", flush=True)
        mock_print.assert_any_call(" tok2", end="", flush=True)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_prompt_raises(self):
        with self.assertRaises(ValueError):
            of.generate("")

    def test_whitespace_only_prompt_raises(self):
        with self.assertRaises(ValueError):
            of.generate("   \t\n")

    def test_very_long_prompt(self):
        ollama_mock.generate.return_value = {"response": "long reply"}
        result = of.generate("x" * 100_000)
        self.assertEqual(result, "long reply")

    def test_temperature_zero(self):
        ollama_mock.generate.return_value = {"response": "deterministic"}
        of.generate("q", temperature=0.0)
        args = ollama_mock.generate.call_args
        self.assertEqual(args.kwargs["options"]["temperature"], 0.0)

    def test_max_tokens_one(self):
        ollama_mock.generate.return_value = {"response": "x"}
        of.generate("q", max_tokens=1)
        args = ollama_mock.generate.call_args
        self.assertEqual(args.kwargs["options"]["num_predict"], 1)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.chat()
# ─────────────────────────────────────────────────────────────────────────────
class TestChat(unittest.TestCase):

    def setUp(self):
        _reset()
        # Each test gets a fresh conversation instance
        self.conv = of.OllamaChat()

    def _mock_reply(self, text):
        """Configure the mock to return `text` as the next chat response."""
        ollama_mock.chat.return_value = {"message": {"content": text}}

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_reply_string(self):
        self._mock_reply("answer")
        result = self.conv.chat("question")
        self.assertEqual(result, "answer")

    def test_history_grows_with_each_call(self):
        self._mock_reply("reply1")
        self.conv.chat("msg1")
        self._mock_reply("reply2")
        self.conv.chat("msg2")
        roles = [m["role"] for m in self.conv.get_history()]
        self.assertEqual(roles, ["system", "user", "assistant", "user", "assistant"])

    def test_user_message_appended_before_api_call(self):
        """The history sent to Ollama must already include the user message."""
        captured = []

        def capture(**kwargs):
            # Snapshot the list at call time (before the assistant reply is added)
            captured.append([m.copy() for m in kwargs["messages"]])
            return {"message": {"content": "ok"}}

        ollama_mock.chat.side_effect = capture
        self.conv.chat("hello")
        sent_messages = captured[0]
        self.assertEqual(sent_messages[-1]["role"], "user")
        self.assertEqual(sent_messages[-1]["content"], "hello")

    def test_stream_mode_returns_full_reply(self):
        chunks = [{"message": {"content": "a"}}, {"message": {"content": "b"}}]
        ollama_mock.chat.return_value = iter(chunks)
        with patch("builtins.print"):
            result = self.conv.chat("hi", stream=True)
        self.assertEqual(result, "ab")

    def test_assistant_reply_added_to_history(self):
        self._mock_reply("the answer")
        self.conv.chat("question")
        history = self.conv.get_history()
        self.assertEqual(history[-1]["role"], "assistant")
        self.assertEqual(history[-1]["content"], "the answer")

    def test_two_conversations_are_independent(self):
        """Two OllamaChat instances must not share history."""
        conv1 = of.OllamaChat()
        conv2 = of.OllamaChat()
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": "title"}
        conv1.chat("message for conv1")
        # conv2 should still have only the system prompt — untouched by conv1
        self.assertEqual(len(conv2.get_history()), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_message_raises(self):
        with self.assertRaises(ValueError):
            self.conv.chat("")

    def test_whitespace_only_message_raises(self):
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
        ollama_mock.chat.return_value = {"message": {"content": "r"}}
        ollama_mock.generate.return_value = {"response": "Some Title"}
        self.conv.chat("something")
        self.conv.clear_history()
        history = self.conv.get_history()
        self.assertEqual(len(history), 1)
        self.assertEqual(history[0]["role"], "system")
        self.assertEqual(history[0]["content"], of.SYSTEM_PROMPT)

    def test_clear_resets_title(self):
        ollama_mock.chat.return_value = {"message": {"content": "r"}}
        ollama_mock.generate.return_value = {"response": "Some Title"}
        self.conv.chat("something")
        self.assertIsNotNone(self.conv.get_title())
        self.conv.clear_history()
        self.assertIsNone(self.conv.get_title())

    def test_clear_on_fresh_history_is_idempotent(self):
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
        """Mutating the returned list must not affect the conversation state."""
        history = self.conv.get_history()
        history.append({"role": "user", "content": "injected"})
        self.assertEqual(len(self.conv.get_history()), 1)  # internal state unchanged


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.save_history() / OllamaChat.load_history()
# ─────────────────────────────────────────────────────────────────────────────
class TestSaveLoadHistory(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_save_then_load_roundtrip(self):
        ollama_mock.chat.return_value = {"message": {"content": "hi"}}
        ollama_mock.generate.return_value = {"response": "My Title"}
        self.conv.chat("hello")
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            self.conv.save_history(path)
            self.conv.clear_history()
            self.assertIsNone(self.conv.get_title())
            self.conv.load_history(path)
            self.assertGreater(len(self.conv.get_history()), 1)
            self.assertEqual(self.conv.get_title(), "My Title")
        finally:
            os.unlink(path)

    def test_load_old_format_list(self):
        """Files saved before titles were added (plain list) load without error."""
        old_data = [
            {"role": "system", "content": "sys"},
            {"role": "user", "content": "hi"},
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

    def test_save_default_path_creates_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            with patch("os.path.expanduser", return_value=tmpdir):
                with patch("ollama_functions.os.makedirs"):
                    with patch("builtins.open", unittest.mock.mock_open()):
                        with patch("json.dump") as mock_dump:
                            self.conv.save_history()
                            mock_dump.assert_called_once()

    def test_load_missing_file_raises(self):
        with self.assertRaises(FileNotFoundError):
            self.conv.load_history("/nonexistent/path/session.json")

    def test_load_corrupted_json_raises(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as tmp:
            tmp.write("not valid json {{{{")
            path = tmp.name
        try:
            with self.assertRaises(json.JSONDecodeError):
                self.conv.load_history(path)
        finally:
            os.unlink(path)

    def test_save_returns_filepath(self):
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            returned = self.conv.save_history(path)
            self.assertEqual(returned, path)
        finally:
            os.unlink(path)

    def test_load_into_separate_instance(self):
        """A saved conversation can be loaded into a different OllamaChat instance."""
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": "Loaded Title"}
        self.conv.chat("original question")
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            path = tmp.name
        try:
            self.conv.save_history(path)
            new_conv = of.OllamaChat()
            new_conv.load_history(path)
            self.assertEqual(new_conv.get_title(), "Loaded Title")
            self.assertGreater(len(new_conv.get_history()), 1)
        finally:
            os.unlink(path)


# ─────────────────────────────────────────────────────────────────────────────
# OllamaChat.get_title() / OllamaChat.set_title() / auto-title
# ─────────────────────────────────────────────────────────────────────────────
class TestTitle(unittest.TestCase):

    def setUp(self):
        _reset()
        self.conv = of.OllamaChat()

    def test_title_none_before_any_chat(self):
        self.assertIsNone(self.conv.get_title())

    def test_title_auto_generated_after_first_chat(self):
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": "Auto Title"}
        self.conv.chat("first message")
        self.assertEqual(self.conv.get_title(), "Auto Title")

    def test_title_not_regenerated_on_second_message(self):
        """generate() should only be called once — for the first message title."""
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": "First Title"}
        self.conv.chat("first message")
        ollama_mock.generate.return_value = {"response": "Second Title"}
        self.conv.chat("second message")
        self.assertEqual(ollama_mock.generate.call_count, 1)
        self.assertEqual(self.conv.get_title(), "First Title")

    def test_set_title_overwrites_auto_title(self):
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": "Auto Title"}
        self.conv.chat("first message")
        self.conv.set_title("My Custom Title")
        self.assertEqual(self.conv.get_title(), "My Custom Title")

    def test_set_title_strips_whitespace(self):
        self.conv.set_title("  padded  ")
        self.assertEqual(self.conv.get_title(), "padded")

    def test_set_title_before_any_chat(self):
        self.conv.set_title("Pre-set Title")
        self.assertEqual(self.conv.get_title(), "Pre-set Title")

    def test_set_title_empty_raises(self):
        with self.assertRaises(ValueError):
            self.conv.set_title("")

    def test_set_title_whitespace_raises(self):
        with self.assertRaises(ValueError):
            self.conv.set_title("   ")

    def test_auto_title_strips_quotes(self):
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.return_value = {"response": '"Quoted Title"'}
        self.conv.chat("first message")
        self.assertEqual(self.conv.get_title(), "Quoted Title")

    def test_auto_title_falls_back_on_generate_error(self):
        ollama_mock.chat.return_value = {"message": {"content": "reply"}}
        ollama_mock.generate.side_effect = Exception("Ollama down")
        self.conv.chat("explain warp divergence in CUDA kernels")
        self.assertIsNotNone(self.conv.get_title())
        self.assertIn("warp divergence", self.conv.get_title())

    def test_two_instances_have_independent_titles(self):
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
        _reset()
        ollama_mock.embeddings.return_value = {"embedding": [0.1, 0.2, 0.3]}

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_numpy_array(self):
        result = of.embed("hello")
        self.assertIsInstance(result, np.ndarray)

    def test_vector_values_match_response(self):
        result = of.embed("hello")
        np.testing.assert_array_almost_equal(result, [0.1, 0.2, 0.3])

    def test_caching_avoids_second_api_call(self):
        of.embed("same text")
        of.embed("same text")
        self.assertEqual(ollama_mock.embeddings.call_count, 1)

    def test_use_cache_false_always_calls_api(self):
        of.embed("text", use_cache=False)
        of.embed("text", use_cache=False)
        self.assertEqual(ollama_mock.embeddings.call_count, 2)

    def test_different_texts_both_cached(self):
        of.embed("text A")
        of.embed("text B")
        self.assertEqual(ollama_mock.embeddings.call_count, 2)
        of.embed("text A")  # should hit the cache
        self.assertEqual(ollama_mock.embeddings.call_count, 2)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_empty_text_raises(self):
        with self.assertRaises(ValueError):
            of.embed("")

    def test_whitespace_only_raises(self):
        with self.assertRaises(ValueError):
            of.embed("   ")

    def test_single_character_text(self):
        result = of.embed("x")
        self.assertIsInstance(result, np.ndarray)


# ─────────────────────────────────────────────────────────────────────────────
# similarity() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSimilarity(unittest.TestCase):

    def setUp(self):
        _reset()

    def _set_embeddings(self, vec_a, vec_b):
        """Return vec_a on the first embed call, vec_b on the second."""
        ollama_mock.embeddings.side_effect = [
            {"embedding": vec_a},
            {"embedding": vec_b},
        ]

    # Happy path ──────────────────────────────────────────────────────────────

    def test_identical_vectors_score_one(self):
        v = [1.0, 0.0, 0.0]
        self._set_embeddings(v, v)
        score = of.similarity("a", "b")
        self.assertAlmostEqual(score, 1.0, places=3)

    def test_orthogonal_vectors_score_zero(self):
        self._set_embeddings([1.0, 0.0], [0.0, 1.0])
        score = of.similarity("a", "b")
        self.assertAlmostEqual(score, 0.0, places=3)

    def test_opposite_vectors_score_minus_one(self):
        self._set_embeddings([1.0, 0.0], [-1.0, 0.0])
        score = of.similarity("a", "b")
        self.assertAlmostEqual(score, -1.0, places=3)

    def test_returns_float(self):
        self._set_embeddings([1.0, 2.0], [2.0, 1.0])
        result = of.similarity("x", "y")
        self.assertIsInstance(result, float)

    def test_score_rounded_to_4_decimal_places(self):
        self._set_embeddings([1.0, 2.0, 3.0], [3.0, 2.0, 1.0])
        score = of.similarity("x", "y")
        self.assertEqual(score, round(score, 4))


# ─────────────────────────────────────────────────────────────────────────────
# search() — stateless module-level function
# ─────────────────────────────────────────────────────────────────────────────
class TestSearch(unittest.TestCase):

    def setUp(self):
        _reset()

    def _embed_side_effect(self, vectors):
        """Map each successive embed() call to the next vector in the list."""
        it = iter(vectors)
        ollama_mock.embeddings.side_effect = lambda **kw: {"embedding": next(it)}

    # Happy path ──────────────────────────────────────────────────────────────

    def test_returns_top_n_results(self):
        # 1 query + 3 items = 4 embed calls
        self._embed_side_effect([
            [1.0, 0.0],   # query
            [0.9, 0.1],   # item 0 — most similar
            [0.0, 1.0],   # item 1
            [-1.0, 0.0],  # item 2 — least similar
        ])
        results = of.search("q", ["a", "b", "c"], ["t1", "t2", "t3"], top_n=2)
        self.assertEqual(len(results), 2)

    def test_results_sorted_best_first(self):
        self._embed_side_effect([
            [1.0, 0.0],
            [-1.0, 0.0],  # opposite — score -1
            [1.0, 0.0],   # identical — score  1
        ])
        results = of.search("q", ["low", "high"], ["t1", "t2"], top_n=2)
        self.assertGreater(results[0][0], results[1][0])

    def test_result_tuples_contain_score_and_label(self):
        self._embed_side_effect([
            [1.0, 0.0],
            [1.0, 0.0],
        ])
        results = of.search("q", ["myfile.cu"], ["content"], top_n=1)
        score, label = results[0]
        self.assertIsInstance(score, float)
        self.assertEqual(label, "myfile.cu")

    def test_top_n_larger_than_list_returns_all(self):
        self._embed_side_effect([
            [1.0, 0.0],
            [0.9, 0.1],
        ])
        results = of.search("q", ["a"], ["t1"], top_n=100)
        self.assertEqual(len(results), 1)

    def test_top_n_one_returns_single_best(self):
        self._embed_side_effect([
            [1.0, 0.0],
            [0.5, 0.5],
            [0.0, 1.0],
        ])
        results = of.search("q", ["a", "b"], ["t1", "t2"], top_n=1)
        self.assertEqual(len(results), 1)

    # Edge cases ──────────────────────────────────────────────────────────────

    def test_mismatched_labels_texts_raises(self):
        with self.assertRaises(ValueError):
            of.search("q", ["a", "b"], ["only_one"])

    def test_empty_labels_raises(self):
        with self.assertRaises(ValueError):
            of.search("q", [], [])


if __name__ == "__main__":
    unittest.main()
