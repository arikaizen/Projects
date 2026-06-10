"""web_search_scholar — Academic search via Semantic Scholar public API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    query  = args["query"]
    limit  = min(int(args.get("limit", 5)), 10)
    fields = "title,authors,year,abstract,url,citationCount"
    params = urllib.parse.urlencode({"query": query, "limit": limit, "fields": fields})
    url    = f"https://api.semanticscholar.org/graph/v1/paper/search?{params}"
    req    = urllib.request.Request(url, headers={"User-Agent": "MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        data = json.loads(r.read())
    papers = []
    for p in data.get("data", []):
        papers.append({
            "title":    p.get("title"),
            "year":     p.get("year"),
            "authors":  [a["name"] for a in p.get("authors", [])[:3]],
            "abstract": (p.get("abstract") or "")[:400],
            "url":      p.get("url"),
            "citations": p.get("citationCount"),
        })
    return papers

run(handler)
