"""dad_joke — Random dad joke from icanhazdadjoke.com."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    req = urllib.request.Request(
        "https://icanhazdadjoke.com/",
        headers={"Accept": "application/json", "User-Agent": "MCPBot/1.0"}
    )
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return {"joke": d.get("joke"), "id": d.get("id")}

run(handler)
