"""unsplash_search — Search high-quality photos (Unsplash API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    access_key = keys["UNSPLASH_ACCESS_KEY"]
    params = urllib.parse.urlencode({"query": args["query"],
                                      "per_page": min(int(args.get("per_page",5)),10),
                                      "orientation": args.get("orientation","landscape")})
    url = f"https://api.unsplash.com/search/photos?{params}"
    req = urllib.request.Request(url, headers={"Authorization": f"Client-ID {access_key}",
                                                "User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return [{"id":p["id"],"description":p.get("description") or p.get("alt_description",""),
             "width":p["width"],"height":p["height"],
             "urls":{"small":p["urls"]["small"],"regular":p["urls"]["regular"]},
             "photographer":p["user"]["name"],"download_url":p["links"]["download"]}
            for p in d.get("results",[])]

run(handler)
