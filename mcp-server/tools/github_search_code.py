"""github_search_code — Search code on GitHub."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token  = keys.get("GITHUB_TOKEN","")
    params = urllib.parse.urlencode({"q": args["query"],
                                      "per_page": min(int(args.get("per_page",5)),10)})
    d = _gh(f"https://api.github.com/search/code?{params}", token)
    return [{"name":i["name"],"path":i["path"],"repo":i["repository"]["full_name"],
             "url":i["html_url"]} for i in d.get("items",[])]

run(handler)
