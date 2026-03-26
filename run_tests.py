#!/usr/bin/env python3
"""
run_tests.py
─────────────────────────────────────────────────────────────────────────────
Runs the full integration test suite for ollama_functions.py.
Requires Ollama to be running locally with the configured model available.

Usage:
    python run_tests.py
    python run_tests.py -v        # verbose: show each test name as it runs
    python run_tests.py -f        # failfast: stop on first failure
"""

import sys
import unittest

# ── Load the test module ──────────────────────────────────────────────────────

# Import every test class from the test file so the runner can discover them
from test_ollama_functions import (
    TestGenerate,
    TestChat,
    TestClearHistory,
    TestGetHistory,
    TestSaveLoadHistory,
    TestTitle,
    TestEmbed,
    TestSimilarity,
    TestSearch,
    TestGenerateErrors,
    TestChatErrors,
    TestOllamaChatInitErrors,
    TestLoadHistoryErrors,
    TestSaveHistoryErrors,
)

# ── Assemble the suite ────────────────────────────────────────────────────────

# Create an empty suite that we'll fill with tests from each class
suite = unittest.TestSuite()

# Load all test methods from each class and add them to the suite.
# TestLoader discovers methods whose names start with "test_" automatically.
loader = unittest.TestLoader()

# generate() — stateless single-prompt function
suite.addTests(loader.loadTestsFromTestCase(TestGenerate))

# OllamaChat.chat() — multi-turn conversation
suite.addTests(loader.loadTestsFromTestCase(TestChat))

# OllamaChat.clear_history() — wiping conversation state
suite.addTests(loader.loadTestsFromTestCase(TestClearHistory))

# OllamaChat.get_history() — reading conversation state safely
suite.addTests(loader.loadTestsFromTestCase(TestGetHistory))

# OllamaChat.save_history() / load_history() — persistence to disk
suite.addTests(loader.loadTestsFromTestCase(TestSaveLoadHistory))

# OllamaChat.get_title() / set_title() / auto-title generation
suite.addTests(loader.loadTestsFromTestCase(TestTitle))

# embed() — converting text to a vector
suite.addTests(loader.loadTestsFromTestCase(TestEmbed))

# similarity() — comparing two texts by meaning
suite.addTests(loader.loadTestsFromTestCase(TestSimilarity))

# search() — finding the most relevant item from a list
suite.addTests(loader.loadTestsFromTestCase(TestSearch))

# ── Error handling — invalid parameters, bad files, corrupt data ──────────────

# generate() — invalid temperature and max_tokens values
suite.addTests(loader.loadTestsFromTestCase(TestGenerateErrors))

# OllamaChat.chat() — invalid parameters and history rollback on failure
suite.addTests(loader.loadTestsFromTestCase(TestChatErrors))

# OllamaChat.__init__() — invalid model name and system prompt
suite.addTests(loader.loadTestsFromTestCase(TestOllamaChatInitErrors))

# OllamaChat.load_history() — corrupt, malformed, and wrong-type JSON files
suite.addTests(loader.loadTestsFromTestCase(TestLoadHistoryErrors))

# OllamaChat.save_history() — unwritable paths
suite.addTests(loader.loadTestsFromTestCase(TestSaveHistoryErrors))

# ── Parse CLI flags ───────────────────────────────────────────────────────────

# Check for -v / --verbose flag to show each test name as it runs
verbosity = 2 if ("-v" in sys.argv or "--verbose" in sys.argv) else 1

# Check for -f / --failfast flag to stop on the first failure
failfast = "-f" in sys.argv or "--failfast" in sys.argv

# ── Run ───────────────────────────────────────────────────────────────────────

# Print a header so the output is easy to read
print("=" * 70)
print("ollama_functions — integration test suite")
print("Make sure Ollama is running before starting.")
print("=" * 70)

# Create the runner and execute the suite
runner = unittest.TextTestRunner(verbosity=verbosity, failfast=failfast)
result = runner.run(suite)

# ── Exit code ─────────────────────────────────────────────────────────────────

# Exit with code 1 if any tests failed or errored — useful for CI pipelines
# Exit with code 0 if everything passed
sys.exit(0 if result.wasSuccessful() else 1)
