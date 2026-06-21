"""country_list — List all countries with optional region filter."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    url = "https://restcountries.com/v3.1/all?fields=name,cca2,region,population"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    region_filter = args.get("region", "").strip().lower()
    countries = []
    for c in data:
        region = c.get("region", "")
        if region_filter and region.lower() != region_filter:
            continue
        countries.append({
            "name": c.get("name", {}).get("common"),
            "cca2": c.get("cca2"),
            "region": region,
            "population": c.get("population"),
        })
    countries.sort(key=lambda x: x["name"] or "")
    return countries

run(handler)
