"""maps_elevation — Get elevation for a location."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key      = keys["GOOGLE_API_KEY"]
    locations = f"{args['lat']},{args['lng']}"
    params   = urllib.parse.urlencode({"key": key, "locations": locations})
    url      = f"https://maps.googleapis.com/maps/api/elevation/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    results = data.get("results", [])
    if not results: raise ValueError("No elevation data")
    r = results[0]
    return {"elevation_meters": r.get("elevation"), "resolution_meters": r.get("resolution")}

run(handler)
