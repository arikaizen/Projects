"""hackernews_top — HackerNews top stories."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    limit = min(int(args.get("limit", 10)), 30)
    with urllib.request.urlopen("https://hacker-news.firebaseio.com/v0/topstories.json", timeout=15) as r:
        ids = json.loads(r.read())
    stories = []
    for item_id in ids[:limit]:
        url = f"https://hacker-news.firebaseio.com/v0/item/{item_id}.json"
        with urllib.request.urlopen(url, timeout=15) as r:
            item = json.loads(r.read())
        if item:
            stories.append({
                "title": item.get("title"),
                "score": item.get("score"),
                "url": item.get("url"),
                "by": item.get("by"),
                "descendants": item.get("descendants"),
            })
    return stories

run(handler)
