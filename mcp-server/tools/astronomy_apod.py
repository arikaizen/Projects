"""astronomy_apod — NASA Astronomy Picture of the Day."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    api_key = keys.get("NASA_API_KEY", "DEMO_KEY")
    params = {"api_key": api_key}
    if "date" in args:
        params["date"] = args["date"]
    if "count" in args:
        params["count"] = args["count"]
    url = "https://api.nasa.gov/planetary/apod?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    if isinstance(d, list):
        return [{"title": item.get("title"), "explanation": item.get("explanation"),
                 "url": item.get("url"), "media_type": item.get("media_type")} for item in d]
    return {
        "title": d.get("title"),
        "explanation": d.get("explanation"),
        "url": d.get("url"),
        "media_type": d.get("media_type"),
    }

run(handler)
