"""github_list_prs — List pull requests in a repository."""
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
    params = urllib.parse.urlencode({"state": args.get("state","open"),
                                      "per_page": min(int(args.get("per_page",10)),30)})
    d = _gh(f"https://api.github.com/repos/{repo}/pulls?{params}", token)
    return [{"number":p["number"],"title":p["title"],"state":p["state"],
             "author":p["user"]["login"],"url":p["html_url"],
             "draft":p["draft"],"created_at":p["created_at"]} for p in d]

run(handler)
