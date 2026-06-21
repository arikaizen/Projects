"""calendar_holidays — Public holidays for a country and year."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json
from datetime import datetime

def handler(args, keys):
    country_code = args["country_code"].upper()
    year = int(args.get("year", datetime.now().year))
    url = f"https://date.nager.at/api/v3/PublicHolidays/{year}/{country_code}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return [{"date": h.get("date"), "name": h.get("name"),
             "localName": h.get("localName"), "type": h.get("types", [])} for h in data]

run(handler)
