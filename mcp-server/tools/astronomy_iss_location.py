"""astronomy_iss_location — ISS real-time position."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    url = "http://api.open-notify.org/iss-now.json"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    pos = d.get("iss_position", {})
    return {
        "latitude": pos.get("latitude"),
        "longitude": pos.get("longitude"),
        "timestamp": d.get("timestamp"),
    }

run(handler)
