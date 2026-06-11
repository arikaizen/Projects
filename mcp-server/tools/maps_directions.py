"""maps_directions — Get directions between two places."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["GOOGLE_API_KEY"]
    params = urllib.parse.urlencode({
        "key":         key,
        "origin":      args["origin"],
        "destination": args["destination"],
        "mode":        args.get("mode", "driving"),
    })
    url = f"https://maps.googleapis.com/maps/api/directions/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    if data["status"] != "OK":
        raise ValueError(f"Directions failed: {data['status']}")
    route = data["routes"][0]
    leg   = route["legs"][0]
    return {
        "summary":       route.get("summary"),
        "distance":      leg["distance"]["text"],
        "duration":      leg["duration"]["text"],
        "start_address": leg["start_address"],
        "end_address":   leg["end_address"],
        "steps": [{"instruction": s["html_instructions"].replace("<b>","").replace("</b>",""),
                   "distance": s["distance"]["text"]} for s in leg["steps"]],
    }

run(handler)
