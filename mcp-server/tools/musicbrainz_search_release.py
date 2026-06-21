"""musicbrainz_search_release — Search for releases on MusicBrainz."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    query = args["query"]
    artist = args.get("artist", "")
    limit = int(args.get("limit", 5))
    q = f"release:{query}"
    if artist:
        q += f" AND artist:{artist}"
    params = urllib.parse.urlencode({"query": q, "limit": limit, "fmt": "json"})
    url = f"https://musicbrainz.org/ws/2/release/?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "MCPBot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    results = []
    for rel in d.get("releases", []):
        artist_credits = [ac.get("name") or ac.get("artist", {}).get("name")
                         for ac in rel.get("artist-credit", [])]
        label_info = rel.get("label-info", [])
        labels = [li.get("label", {}).get("name") for li in label_info if li.get("label")]
        results.append({
            "title": rel.get("title"),
            "date": rel.get("date"),
            "country": rel.get("country"),
            "artist_credit": artist_credits,
            "label": labels,
        })
    return results

run(handler)
