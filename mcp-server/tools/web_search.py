"""web_search — Google Custom Search API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    api_key = keys.get("GOOGLE_API_KEY") or (_ for _ in ()).throw(ValueError("GOOGLE_API_KEY required"))
    cse_id  = keys.get("GOOGLE_CSE_ID")  or (_ for _ in ()).throw(ValueError("GOOGLE_CSE_ID required"))
    query   = args["query"]
    num     = min(int(args.get("num", 5)), 10)
    start   = int(args.get("start", 1))
    params  = urllib.parse.urlencode({"key": api_key, "cx": cse_id, "q": query, "num": num, "start": start})
    url     = f"https://www.googleapis.com/customsearch/v1?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    items = data.get("items", [])
    return [{"title": i.get("title"), "link": i.get("link"), "snippet": i.get("snippet")} for i in items]

run(handler)
