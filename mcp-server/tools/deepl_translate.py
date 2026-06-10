"""deepl_translate — High-quality translation via DeepL API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    api_key = keys["DEEPL_API_KEY"]
    # free-tier uses api-free.deepl.com
    base = "https://api-free.deepl.com" if api_key.endswith(":fx") else "https://api.deepl.com"
    params = urllib.parse.urlencode({
        "auth_key": api_key,
        "text": args["text"],
        "target_lang": args["target_lang"].upper(),
        **( {"source_lang": args["source_lang"].upper()} if args.get("source_lang") else {} )
    }).encode()
    req = urllib.request.Request(f"{base}/v2/translate", data=params, method="POST",
                                  headers={"Content-Type":"application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    t = d["translations"][0]
    return {"translated_text": t["text"],
            "detected_source_language": t.get("detected_source_language","")}

run(handler)
