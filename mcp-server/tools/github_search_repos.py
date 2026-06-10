"""github_search_repos — Search GitHub repositories."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token  = keys.get("GITHUB_TOKEN","")
    params = urllib.parse.urlencode({"q": args["query"], "sort": args.get("sort","stars"),
                                      "per_page": min(int(args.get("per_page",5)),10)})
    d = _gh(f"https://api.github.com/search/repositories?{params}", token)
    return [{"name":r["full_name"],"description":r["description"],"stars":r["stargazers_count"],
             "language":r["language"],"url":r["html_url"]} for r in d.get("items",[])]

run(handler)
