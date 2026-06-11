"""maps_place_details — Get detailed info about a place."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key      = keys["GOOGLE_API_KEY"]
    place_id = args["place_id"]
    fields   = args.get("fields", "name,formatted_address,formatted_phone_number,website,rating,opening_hours,price_level")
    params   = urllib.parse.urlencode({"key": key, "place_id": place_id, "fields": fields})
    url      = f"https://maps.googleapis.com/maps/api/place/details/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return data.get("result", {})

run(handler)
