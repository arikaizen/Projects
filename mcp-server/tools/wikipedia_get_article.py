"""wikipedia_get_article — Get full Wikipedia article text."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    title  = args["title"]
    params = urllib.parse.urlencode({"action":"query","prop":"extracts","explaintext":1,
                                      "titles":title,"format":"json","utf8":1})
    url = f"https://en.wikipedia.org/w/api.php?{params}"
    req = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        d = json.loads(r.read())
    pages = d["query"]["pages"]
    page  = next(iter(pages.values()))
    return {"title":page.get("title"),"extract":(page.get("extract",""))[:8000]}

run(handler)
