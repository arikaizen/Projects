"""musicbrainz_search_artist — Search for artists on MusicBrainz."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    query = args["query"]
    limit = int(args.get("limit", 5))
    params = urllib.parse.urlencode({"query": query, "limit": limit, "fmt": "json"})
    url = f"https://musicbrainz.org/ws/2/artist/?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "MCPBot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    results = []
    for a in d.get("artists", []):
        results.append({
            "id": a.get("id"),
            "name": a.get("name"),
            "country": a.get("country"),
            "type": a.get("type"),
            "score": a.get("score"),
        })
    return results

run(handler)
