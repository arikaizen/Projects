"""wikipedia_get_summary — Get a Wikipedia article summary."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    title  = urllib.parse.quote(args["title"].replace(" ","_"))
    url    = f"https://en.wikipedia.org/api/rest_v1/page/summary/{title}"
    req    = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return {"title":d.get("title"),"description":d.get("description"),
            "extract":d.get("extract","")[:2000],"url":d.get("content_urls",{}).get("desktop",{}).get("page")}

run(handler)
