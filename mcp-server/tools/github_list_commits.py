"""github_list_commits — List recent commits in a repository."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token  = keys.get("GITHUB_TOKEN","")
    repo   = args["repo"]
    params = urllib.parse.urlencode({"per_page": min(int(args.get("per_page",10)),30),
                                      "sha": args.get("branch","")})
    d = _gh(f"https://api.github.com/repos/{repo}/commits?{params}", token)
    return [{"sha":c["sha"][:8],"message":c["commit"]["message"].split("\n")[0],
             "author":c["commit"]["author"]["name"],
             "date":c["commit"]["author"]["date"]} for c in d]

run(handler)
