"""web_fetch_page — Fetch plain text / HTML from a URL."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, html.parser, re

class _MLStripper(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self._buf = []
    def handle_data(self, d): self._buf.append(d)
    def get_text(self): return " ".join(self._buf)

def handler(args, keys):
    url     = args["url"]
    as_text = args.get("as_text", True)
    req     = urllib.request.Request(url, headers={"User-Agent": "MCP-Bot/1.0"})
    with urllib.request.urlopen(req, timeout=20) as r:
        raw = r.read().decode(r.headers.get_content_charset() or "utf-8", errors="replace")
    if as_text:
        s = _MLStripper(); s.feed(raw)
        text = re.sub(r'\s+', ' ', s.get_text()).strip()
        return {"url": url, "text": text[:8000]}
    return {"url": url, "html": raw[:8000]}

run(handler)
