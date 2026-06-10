"""weather_alerts — Severe weather alerts via OpenWeatherMap One Call API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["OPENWEATHER_API_KEY"]
    params = urllib.parse.urlencode({"appid": key, "lat": args["lat"], "lon": args["lon"],
                                      "exclude": "current,minutely,hourly,daily"})
    url = f"https://api.openweathermap.org/data/3.0/onecall?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    alerts = d.get("alerts", [])
    return [{"sender": a.get("sender_name"), "event": a.get("event"),
             "description": a.get("description","")[:500]} for a in alerts]

run(handler)
