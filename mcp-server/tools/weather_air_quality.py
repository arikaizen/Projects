"""weather_air_quality — Air quality index (OpenWeatherMap Air Pollution API)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

AQI_LABELS = {1:"Good",2:"Fair",3:"Moderate",4:"Poor",5:"Very Poor"}

def handler(args, keys):
    key    = keys["OPENWEATHER_API_KEY"]
    params = urllib.parse.urlencode({"appid": key, "lat": args["lat"], "lon": args["lon"]})
    url    = f"https://api.openweathermap.org/data/2.5/air_pollution?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    item = d["list"][0]
    aqi  = item["main"]["aqi"]
    return {"aqi": aqi, "label": AQI_LABELS.get(aqi,"Unknown"), "components": item["components"]}

run(handler)
