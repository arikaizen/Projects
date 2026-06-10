"""wikipedia_search — Search Wikipedia articles."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    params = urllib.parse.urlencode({"action":"query","list":"search","srsearch":args["query"],
                                      "srlimit":min(int(args.get("limit",5)),10),
                                      "format":"json","utf8":1})
    url = f"https://en.wikipedia.org/w/api.php?{params}"
    req = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return [{"title":s["title"],"snippet":s["snippet"].replace("<span class=\"searchmatch\">","").replace("</span>",""),
             "pageid":s["pageid"]} for s in d["query"]["search"]]

run(handler)
