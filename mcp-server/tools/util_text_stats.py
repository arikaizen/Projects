"""util_text_stats — Compute text statistics."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import re

def handler(args, keys):
    text = args["text"]
    words = re.findall(r"\b\w+\b", text)
    sentences = re.split(r"[.!?]+", text)
    sentences = [s for s in sentences if s.strip()]
    paragraphs = [p for p in re.split(r"\n\s*\n", text) if p.strip()]
    if not paragraphs and text.strip():
        paragraphs = [text.strip()]
    char_no_spaces = len(text.replace(" ", "").replace("\n", "").replace("\t", ""))
    unique_words = len(set(w.lower() for w in words))
    word_count = len(words)
    reading_time_seconds = round(word_count / 200 * 60)  # ~200 wpm
    return {
        "word_count": word_count,
        "char_count": len(text),
        "char_no_spaces": char_no_spaces,
        "sentence_count": len(sentences),
        "paragraph_count": len(paragraphs),
        "reading_time_seconds": reading_time_seconds,
        "unique_words": unique_words,
    }

run(handler)
