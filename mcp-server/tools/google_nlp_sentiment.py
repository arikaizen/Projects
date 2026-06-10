"""google_nlp_sentiment — Analyze sentiment of text (Google Cloud Natural Language API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key  = keys["GOOGLE_API_KEY"]
    text = args["text"]
    body = json.dumps({"document": {"type": "PLAIN_TEXT", "content": text},
                        "encodingType": "UTF8"}).encode()
    url  = f"https://language.googleapis.com/v1/documents:analyzeSentiment?key={key}"
    req  = urllib.request.Request(url, data=body, method="POST",
                                   headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    doc_sentiment = d.get("documentSentiment", {})
    return {"score": doc_sentiment.get("score"),
            "magnitude": doc_sentiment.get("magnitude"),
            "interpretation": ("positive" if doc_sentiment.get("score", 0) > 0.2
                                else "negative" if doc_sentiment.get("score", 0) < -0.2
                                else "neutral")}

run(handler)
