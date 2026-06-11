"""github_get_file — Read a file from a GitHub repository."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json, base64

def _gh(url, token):
    req = urllib.request.Request(url, headers={"Accept":"application/vnd.github+json",
        "User-Agent":"MCP-Bot/1.0", **({"Authorization":f"Bearer {token}"} if token else {})})
    with urllib.request.urlopen(req, timeout=15) as r: return json.loads(r.read())

def handler(args, keys):
    token = keys.get("GITHUB_TOKEN","")
    repo  = args["repo"]
    path  = args["path"]
    ref   = args.get("ref","")
    url   = f"https://api.github.com/repos/{repo}/contents/{path}"
    if ref: url += f"?ref={ref}"
    d = _gh(url, token)
    content = base64.b64decode(d["content"]).decode("utf-8", errors="replace")
    return {"path":d["path"],"size":d["size"],"sha":d["sha"],"content":content[:8000]}

run(handler)
