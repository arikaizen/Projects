"""util_diff — Compute unified diff between two text strings."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import difflib

def handler(args, keys):
    text_a = args["text_a"]
    text_b = args["text_b"]
    context_lines = int(args.get("context_lines", 3))
    label_a = args.get("label_a", "a")
    label_b = args.get("label_b", "b")
    lines_a = text_a.splitlines(keepends=True)
    lines_b = text_b.splitlines(keepends=True)
    diff = list(difflib.unified_diff(lines_a, lines_b, fromfile=label_a, tofile=label_b, n=context_lines))
    return {"diff": "".join(diff), "has_changes": len(diff) > 0}

run(handler)
