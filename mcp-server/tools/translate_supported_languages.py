"""translate_supported_languages — List all languages supported by Google Translate."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["GOOGLE_API_KEY"]
    target = args.get("display_language", "en")
    params = urllib.parse.urlencode({"key": key, "target": target})
    url    = f"https://translation.googleapis.com/language/translate/v2/languages?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return d["data"]["languages"]

run(handler)
