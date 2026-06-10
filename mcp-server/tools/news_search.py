"""news_search — Search all news articles (NewsAPI.org)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["NEWSAPI_KEY"]
    params = urllib.parse.urlencode({"apiKey": key, "q": args["query"],
                                      "language": args.get("language", "en"),
                                      "sortBy": args.get("sort_by", "publishedAt"),
                                      "pageSize": args.get("page_size", 5)})
    url = f"https://newsapi.org/v2/everything?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return [{"title": a["title"], "source": a["source"]["name"],
             "url": a["url"], "published_at": a["publishedAt"],
             "description": a.get("description","")} for a in d.get("articles",[])]

run(handler)
