"""weather_current — Current weather (OpenWeatherMap)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["OPENWEATHER_API_KEY"]
    params = urllib.parse.urlencode({"appid": key, "units": args.get("units", "metric"),
                                      "q": args.get("city", ""),
                                      "lat": args.get("lat", ""), "lon": args.get("lon", "")})
    url = f"https://api.openweathermap.org/data/2.5/weather?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return {"city": d["name"], "country": d["sys"]["country"],
            "temp": d["main"]["temp"], "feels_like": d["main"]["feels_like"],
            "humidity": d["main"]["humidity"], "pressure": d["main"]["pressure"],
            "description": d["weather"][0]["description"], "wind_speed": d["wind"]["speed"],
            "visibility": d.get("visibility")}

run(handler)
