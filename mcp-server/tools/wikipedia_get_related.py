"""wikipedia_get_related — Get links from a Wikipedia article (related articles)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    title  = args["title"]
    limit  = min(int(args.get("limit", 10)), 30)
    params = urllib.parse.urlencode({"action":"query","prop":"links","plnamespace":0,
                                      "pllimit":limit,"titles":title,"format":"json","utf8":1})
    url = f"https://en.wikipedia.org/w/api.php?{params}"
    req = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    pages = d["query"]["pages"]
    page  = next(iter(pages.values()))
    return [l["title"] for l in page.get("links",[])]

run(handler)
