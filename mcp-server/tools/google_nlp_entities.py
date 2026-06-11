"""google_nlp_entities — Extract entities from text (Google Cloud Natural Language API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    key  = keys["GOOGLE_API_KEY"]
    text = args["text"]
    body = json.dumps({"document": {"type": "PLAIN_TEXT", "content": text},
                        "encodingType": "UTF8"}).encode()
    url  = f"https://language.googleapis.com/v1/documents:analyzeEntities?key={key}"
    req  = urllib.request.Request(url, data=body, method="POST",
                                   headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return [{"name": e["name"], "type": e["type"], "salience": e["salience"],
             "wikipedia_url": e.get("metadata",{}).get("wikipedia_url")}
            for e in d.get("entities",[])[:15]]

run(handler)
