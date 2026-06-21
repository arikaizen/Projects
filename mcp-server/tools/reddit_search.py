"""reddit_search — Search Reddit posts."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    query = args["query"]
    sort = args.get("sort", "relevance")
    limit = args.get("limit", 10)
    params = urllib.parse.urlencode({"q": query, "sort": sort, "limit": limit})
    url = f"https://www.reddit.com/search.json?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "MCPBot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    posts = []
    for child in d.get("data", {}).get("children", []):
        p = child.get("data", {})
        posts.append({
            "title": p.get("title"),
            "score": p.get("score"),
            "url": p.get("url"),
            "author": p.get("author"),
            "num_comments": p.get("num_comments"),
            "subreddit": p.get("subreddit"),
        })
    return posts

run(handler)
