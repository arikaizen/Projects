"""maps_distance_matrix — Distance/duration between multiple origins and destinations."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key     = keys["GOOGLE_API_KEY"]
    origins = "|".join(args["origins"])
    dests   = "|".join(args["destinations"])
    params  = urllib.parse.urlencode({"key": key, "origins": origins, "destinations": dests,
                                       "mode": args.get("mode", "driving")})
    url = f"https://maps.googleapis.com/maps/api/distancematrix/json?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return {"origin_addresses": data.get("origin_addresses"),
            "destination_addresses": data.get("destination_addresses"),
            "rows": data.get("rows")}

run(handler)
