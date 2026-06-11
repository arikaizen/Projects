"""wolfram_full_query — Full Wolfram Alpha query with multiple pods (XML parsed)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse
import xml.etree.ElementTree as ET

def handler(args, keys):
    app_id = keys["WOLFRAM_APP_ID"]
    query  = args["query"]
    params = urllib.parse.urlencode({"appid": app_id, "input": query, "format": "plaintext"})
    url    = f"https://api.wolframalpha.com/v2/query?{params}"
    req    = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        xml_data = r.read()
    root = ET.fromstring(xml_data)
    pods = []
    for pod in root.findall("pod"):
        title = pod.get("title","")
        texts = [sp.find("plaintext").text or "" for sp in pod.findall("subpod")
                 if sp.find("plaintext") is not None]
        if any(texts): pods.append({"title": title, "content": " | ".join(texts)})
    return {"query": query, "pods": pods[:8]}

run(handler)
