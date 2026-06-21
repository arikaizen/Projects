"""network_dns_lookup — DNS lookup via Cloudflare DoH."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    domain = args["domain"]
    record_type = args.get("record_type", "A").upper()
    params = urllib.parse.urlencode({"name": domain, "type": record_type})
    url = f"https://cloudflare-dns.com/dns-query?{params}"
    req = urllib.request.Request(url, headers={"Accept": "application/dns-json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        d = json.loads(r.read())
    answers = []
    for a in d.get("Answer", []):
        answers.append({
            "name": a.get("name"),
            "type": a.get("type"),
            "ttl": a.get("TTL"),
            "data": a.get("data"),
        })
    return {
        "domain": domain,
        "record_type": record_type,
        "answers": answers,
        "status": d.get("Status"),
    }

run(handler)
