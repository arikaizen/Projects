"""github_get_user — Get a GitHub user's profile."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token = keys.get("GITHUB_TOKEN","")
    user  = args["username"]
    d = _gh(f"https://api.github.com/users/{user}", token)
    return {"login":d["login"],"name":d.get("name"),"bio":d.get("bio"),
            "company":d.get("company"),"blog":d.get("blog"),
            "public_repos":d["public_repos"],"followers":d["followers"],
            "following":d["following"],"created_at":d["created_at"]}

run(handler)
