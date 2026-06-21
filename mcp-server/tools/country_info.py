"""country_info — Country details from REST Countries API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    name = args["name"]
    url = f"https://restcountries.com/v3.1/name/{urllib.parse.quote(name)}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    c = data[0]
    currencies = {k: v.get("name") for k, v in c.get("currencies", {}).items()}
    languages = list(c.get("languages", {}).values())
    return {
        "name": c.get("name", {}).get("common"),
        "official_name": c.get("name", {}).get("official"),
        "capital": c.get("capital", [None])[0],
        "population": c.get("population"),
        "region": c.get("region"),
        "subregion": c.get("subregion"),
        "currencies": currencies,
        "languages": languages,
        "flag": c.get("flag"),
        "cca2": c.get("cca2"),
    }

run(handler)
