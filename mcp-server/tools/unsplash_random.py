"""unsplash_random — Get a random photo from Unsplash."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    access_key = keys["UNSPLASH_ACCESS_KEY"]
    params = {}
    if args.get("query"):     params["query"]       = args["query"]
    if args.get("orientation"): params["orientation"] = args["orientation"]
    url  = "https://api.unsplash.com/photos/random"
    if params: url += "?" + urllib.parse.urlencode(params)
    req  = urllib.request.Request(url, headers={"Authorization": f"Client-ID {access_key}"})
    with urllib.request.urlopen(req, timeout=15) as r:
        p = json.loads(r.read())
    return {"id":p["id"],"description":p.get("description") or p.get("alt_description",""),
            "width":p["width"],"height":p["height"],
            "url_regular":p["urls"]["regular"],"photographer":p["user"]["name"]}

run(handler)
