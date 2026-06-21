"""network_http_status — Check HTTP status of a URL."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, time

def handler(args, keys):
    url = args["url"]
    start = time.time()
    try:
        req = urllib.request.Request(url, method="HEAD")
        with urllib.request.urlopen(req, timeout=15) as r:
            elapsed_ms = round((time.time() - start) * 1000)
            return {
                "url": url,
                "status_code": r.status,
                "reason": r.reason,
                "redirect_url": r.url if r.url != url else None,
                "response_time_ms": elapsed_ms,
            }
    except urllib.error.HTTPError as e:
        elapsed_ms = round((time.time() - start) * 1000)
        return {
            "url": url,
            "status_code": e.code,
            "reason": e.reason,
            "redirect_url": None,
            "response_time_ms": elapsed_ms,
        }

run(handler)
