"""hackernews_item — Single HackerNews item."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    item_id = int(args["item_id"])
    url = f"https://hacker-news.firebaseio.com/v0/item/{item_id}.json"
    with urllib.request.urlopen(url, timeout=15) as r:
        item = json.loads(r.read())
    return item

run(handler)
