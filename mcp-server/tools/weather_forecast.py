"""weather_forecast — 5-day / 3-hour forecast (OpenWeatherMap)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    key    = keys["OPENWEATHER_API_KEY"]
    params = urllib.parse.urlencode({"appid": key, "units": args.get("units", "metric"),
                                      "q": args.get("city", ""),
                                      "lat": args.get("lat", ""), "lon": args.get("lon", ""),
                                      "cnt": args.get("cnt", 8)})
    url = f"https://api.openweathermap.org/data/2.5/forecast?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    return {"city": d["city"]["name"], "country": d["city"]["country"],
            "forecast": [{"dt_txt": i["dt_txt"], "temp": i["main"]["temp"],
                          "description": i["weather"][0]["description"],
                          "humidity": i["main"]["humidity"]} for i in d["list"]]}

run(handler)
