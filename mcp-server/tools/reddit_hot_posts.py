"""reddit_hot_posts — Reddit hot posts from a subreddit."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    subreddit = args.get("subreddit", "programming")
    limit = args.get("limit", 10)
    url = f"https://www.reddit.com/r/{subreddit}/hot.json?limit={limit}"
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
        })
    return posts

run(handler)
