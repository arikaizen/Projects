"""web_search_images — Google Custom Search image results."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
from _base import run
import urllib.request, urllib.parse, json

def handler(args, keys):
    api_key = keys["GOOGLE_API_KEY"]
    cse_id  = keys["GOOGLE_CSE_ID"]
    query   = args["query"]
    num     = min(int(args.get("num", 5)), 10)
    params  = urllib.parse.urlencode({"key": api_key, "cx": cse_id, "q": query,
                                       "num": num, "searchType": "image"})
    url = f"https://www.googleapis.com/customsearch/v1?{params}"
    with urllib.request.urlopen(url, timeout=15) as r:
        data = json.loads(r.read())
    return [{"title": i.get("title"), "link": i.get("link"),
             "thumbnail": i.get("image",{}).get("thumbnailLink")}
            for i in data.get("items", [])]

run(handler)
