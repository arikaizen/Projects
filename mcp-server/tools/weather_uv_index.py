"""weather_uv_index — Current UV index (OpenWeatherMap)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["OPENWEATHER_API_KEY"]
    params = urllib.parse.urlencode({"appid": key, "lat": args["lat"], "lon": args["lon"]})
    url    = f"https://api.openweathermap.org/data/2.5/uvi?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return {"uv_index": d["value"], "date": d["date_iso"], "lat": d["lat"], "lon": d["lon"]}

run(handler)
