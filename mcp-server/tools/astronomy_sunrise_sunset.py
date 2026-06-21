"""astronomy_sunrise_sunset — Sunrise and sunset times for a location."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    lat = args["lat"]
    lng = args["lng"]
    date = args.get("date", "today")
    params = urllib.parse.urlencode({"lat": lat, "lng": lng, "formatted": 0, "date": date})
    url = f"https://api.sunrise-sunset.org/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    results = d.get("results", {})
    return {
        "sunrise": results.get("sunrise"),
        "sunset": results.get("sunset"),
        "day_length": results.get("day_length"),
    }

run(handler)
