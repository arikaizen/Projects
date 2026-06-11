"""news_sources — List available NewsAPI sources."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["NEWSAPI_KEY"]
    params = urllib.parse.urlencode({"apiKey": key,
                                      "language": args.get("language", "en"),
                                      "country":  args.get("country", ""),
                                      "category": args.get("category", "")})
    url = f"https://newsapi.org/v2/sources?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return [{"id": s["id"], "name": s["name"], "description": s["description"],
             "url": s["url"], "category": s["category"]} for s in d.get("sources",[])]

run(handler)
