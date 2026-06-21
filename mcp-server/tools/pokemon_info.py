"""pokemon_info — Pokémon info from PokeAPI."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    name = str(args["name"]).lower()
    url = f"https://pokeapi.co/api/v2/pokemon/{name}"
    with urllib.request.urlopen(url, timeout=15) as r:
        d = json.loads(r.read())
    stats = {s["stat"]["name"]: s["base_stat"] for s in d.get("stats", [])}
    return {
        "name": d.get("name"),
        "id": d.get("id"),
        "types": [t["type"]["name"] for t in d.get("types", [])],
        "height": d.get("height"),
        "weight": d.get("weight"),
        "base_stats": {
            "hp": stats.get("hp"),
            "attack": stats.get("attack"),
            "defense": stats.get("defense"),
            "speed": stats.get("speed"),
        },
        "abilities": [a["ability"]["name"] for a in d.get("abilities", [])],
    }

run(handler)
