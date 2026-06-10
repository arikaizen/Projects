"""translate_text — Translate text (Google Translate API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["GOOGLE_API_KEY"]
    text   = args["text"]
    target = args["target_language"]
    source = args.get("source_language","")
    params = {"key": key, "q": text, "target": target, "format": "text"}
    if source: params["source"] = source
    data   = urllib.parse.urlencode(params).encode()
    url    = "https://translation.googleapis.com/language/translate/v2"
    req    = urllib.request.Request(url, data=data, method="POST",
                                     headers={"Content-Type":"application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    t = d["data"]["translations"][0]
    return {"translated_text": t["translatedText"],
            "detected_source_language": t.get("detectedSourceLanguage",source)}

run(handler)
