"""util_markdown_to_html — Convert Markdown to HTML (no external deps)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import re

def handler(args, keys):
    md = args["markdown"]
    h  = md
    # Headers
    for i in range(6, 0, -1):
        h = re.sub(r'^#{' + str(i) + r'}\s+(.+)$', rf'<h{i}>\1</h{i}>', h, flags=re.MULTILINE)
    # Bold / italic
    h = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', h)
    h = re.sub(r'\*(.+?)\*',     r'<em>\1</em>', h)
    h = re.sub(r'`(.+?)`',       r'<code>\1</code>', h)
    # Links
    h = re.sub(r'\[(.+?)\]\((.+?)\)', r'<a href="\2">\1</a>', h)
    # Line breaks
    h = h.replace("\n\n", "</p><p>")
    h = f"<p>{h}</p>"
    return {"html": h}

run(handler)
