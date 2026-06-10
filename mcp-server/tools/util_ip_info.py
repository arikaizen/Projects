"""util_ip_info — Get geolocation and ASN info for an IP (ipinfo.io)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, json

def handler(args, keys):
    ip    = args.get("ip", "")
    token = keys.get("IPINFO_TOKEN","")
    url   = f"https://ipinfo.io/{ip}/json" + (f"?token={token}" if token else "")
    req   = urllib.request.Request(url, headers={"User-Agent":"MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    return {k: d.get(k) for k in ["ip","hostname","city","region","country","loc","org","timezone"]}

run(handler)
