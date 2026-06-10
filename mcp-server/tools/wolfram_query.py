"""wolfram_query — Compute answers with Wolfram Alpha Short Answers API."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse

def handler(args, keys):
    app_id = keys["WOLFRAM_APP_ID"]
    query  = args["query"]
    params = urllib.parse.urlencode({"appid": app_id, "i": query})
    url    = f"https://api.wolframalpha.com/v1/result?{params}"
    req    = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        answer = r.read().decode("utf-8")
    return {"query": query, "answer": answer}

run(handler)
