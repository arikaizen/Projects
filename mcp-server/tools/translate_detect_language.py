"""translate_detect_language — Detect language of text (Google Translate API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key   = keys["GOOGLE_API_KEY"]
    text  = args["text"]
    params = urllib.parse.urlencode({"key": key, "q": text}).encode()
    url    = "https://translation.googleapis.com/language/translate/v2/detect"
    req    = urllib.request.Request(url, data=params, method="POST",
                                     headers={"Content-Type":"application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    det = d["data"]["detections"][0][0]
    return {"language": det["language"], "confidence": det["confidence"]}

run(handler)
