"""util_url_encode — URL-encode or URL-decode a string."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.parse

def handler(args, keys):
    text = args["text"]
    mode = args.get("mode", "encode")
    if mode == "encode":
        return {"result": urllib.parse.quote(text, safe=args.get("safe",""))}
    return {"result": urllib.parse.unquote(text)}

run(handler)
