"""maps_geocode — Convert an address to coordinates (Google Maps Geocoding API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key     = keys["GOOGLE_API_KEY"]
    address = args["address"]
    params  = urllib.parse.urlencode({"key": key, "address": address})
    url     = f"https://maps.googleapis.com/maps/api/geocode/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    if data["status"] != "OK":
        raise ValueError(f"Geocoding failed: {data['status']}")
    result = data["results"][0]
    loc    = result["geometry"]["location"]
    return {"formatted_address": result["formatted_address"],
            "lat": loc["lat"], "lng": loc["lng"],
            "place_id": result.get("place_id")}

run(handler)
