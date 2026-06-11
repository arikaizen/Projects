"""maps_nearby_places — Find nearby places using Google Places API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key      = keys["GOOGLE_API_KEY"]
    location = f"{args['lat']},{args['lng']}"
    params   = urllib.parse.urlencode({
        "key":      key,
        "location": location,
        "radius":   args.get("radius", 1000),
        "type":     args.get("type", "restaurant"),
    })
    url = f"https://maps.googleapis.com/maps/api/place/nearbysearch/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return [{"name": p["name"], "address": p.get("vicinity"),
             "rating": p.get("rating"), "types": p.get("types", []),
             "place_id": p.get("place_id")}
            for p in data.get("results", [])[:10]]

run(handler)
