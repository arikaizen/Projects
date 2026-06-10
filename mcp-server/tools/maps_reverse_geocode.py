"""maps_reverse_geocode — Convert coordinates to an address."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["GOOGLE_API_KEY"]
    latlng = f"{args['lat']},{args['lng']}"
    params = urllib.parse.urlencode({"key": key, "latlng": latlng})
    url    = f"https://maps.googleapis.com/maps/api/geocode/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    if data["status"] != "OK":
        raise ValueError(f"Reverse geocoding failed: {data['status']}")
    return {"formatted_address": data["results"][0]["formatted_address"],
            "place_id": data["results"][0].get("place_id"),
            "components": data["results"][0].get("address_components", [])}

run(handler)
