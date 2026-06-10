"""github_get_repo — Get repository details."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token = keys.get("GITHUB_TOKEN","")
    repo  = args["repo"]  # "owner/repo"
    d = _gh(f"https://api.github.com/repos/{repo}", token)
    return {"name":d["full_name"],"description":d.get("description"),"stars":d["stargazers_count"],
            "forks":d["forks_count"],"language":d["language"],"open_issues":d["open_issues_count"],
            "license":d.get("license",{}).get("name") if d.get("license") else None,
            "topics":d.get("topics",[]),"url":d["html_url"]}

run(handler)
