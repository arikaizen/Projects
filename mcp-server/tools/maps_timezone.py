"""maps_timezone — Get timezone info for a lat/lng."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json, time

def handler(args, keys):
    key      = keys["GOOGLE_API_KEY"]
    location = f"{args['lat']},{args['lng']}"
    params   = urllib.parse.urlencode({"key": key, "location": location, "timestamp": int(time.time())})
    url      = f"https://maps.googleapis.com/maps/api/timezone/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return {"timezone_id": data.get("timeZoneId"), "timezone_name": data.get("timeZoneName"),
            "dst_offset": data.get("dstOffset"), "raw_offset": data.get("rawOffset")}

run(handler)
