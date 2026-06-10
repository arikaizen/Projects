"""util_regex_match — Test a regex pattern against text."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import re

def handler(args, keys):
    pattern = args["pattern"]
    text    = args["text"]
    flags   = 0
    if args.get("ignore_case"):  flags |= re.IGNORECASE
    if args.get("multiline"):    flags |= re.MULTILINE
    if args.get("dotall"):       flags |= re.DOTALL
    mode    = args.get("mode", "findall")
    try:
        if mode == "match":
            m = re.match(pattern, text, flags)
            return {"matched": bool(m), "groups": list(m.groups()) if m else [], "span": list(m.span()) if m else []}
        if mode == "search":
            m = re.search(pattern, text, flags)
            return {"found": bool(m), "match": m.group(0) if m else None, "groups": list(m.groups()) if m else []}
        matches = re.findall(pattern, text, flags)
        return {"count": len(matches), "matches": matches[:50]}
    except re.error as e:
        return {"error": str(e)}

run(handler)
